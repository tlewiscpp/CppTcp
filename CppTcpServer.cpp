#include <iostream>
#include <vector>
#include <future>
#include <sstream>
#include <csignal>
#include <cstring>
#include <fstream>
#include <forward_list>
#include <cerrno>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <netdb.h>

#include <unistd.h>

#include "ApplicationUtilities.h"
#include "GlobalDefinitions.h"

#include <getopt.h>
#include <arpa/inet.h>

using namespace ApplicationUtilities;

static int verboseLogging{false};
void globalLogHandler(LogLevel logLevel, LogContext logContext, const std::string &str);
std::map<int, std::future<void>> connections;
void closeConnection(int socketDescriptor);
bool looksLikeIP(const char *str);
bool startsWith(const std::string &str, const std::string &beginning);
std::vector<std::pair<std::string, std::string>> getLocalIP();
std::string getDefaultHostName();

static struct option long_options[]
{
        /* These options set a flag. */
        {"verbose",  no_argument,       &verboseLogging, 1},
        /* These options don’t set a flag, we distinguish them by their indices. */
        {"help",     no_argument,       nullptr, 'h'},
        {"version",  no_argument,       nullptr, 'v'},
        {"port",     required_argument, nullptr, 'p'},
        {"host",     required_argument, nullptr, 'n'},
        {nullptr, 0, nullptr, 0}
};


static const int RECEIVE_TIMEOUT{1500};
static const int MAX_INCOMING_CONNECTIONS{10};
static const int constexpr MINIMUM_PORT_NUMBER{1024};
static const int DEFAULT_PORT_NUMBER{5678};
static int portNumber{-1};
std::string hostName{""};
static const char LINE_ENDING{'\n'};
static const int constexpr BUFFER_MAX{1024};

static std::mutex coutMutex{};

void exitApplication(int exitCode);
void signalHandler(int signal);

void displayVersion();
void displayHelp();

inline std::string stripLineEnding(std::string str) { if ((str.length() > 0) && (str.back() == LINE_ENDING)) str.pop_back(); return str; }
template <char Delimiter = ' '> std::vector<std::string> split(const std::string &str) {
    std::istringstream istr{str};
    std::vector<std::string> returnVector{};
    for (std::string temp{}; std::getline(istr, temp); returnVector.push_back(temp));
    return returnVector;
}

int *socketFileDescriptor{nullptr};
struct timeval toTimeVal(uint32_t totalTimeout);
std::string sockaddrToString(sockaddr *address, socklen_t addressLength);
void printToStdout(const std::string &msg);
void printAddressMessageToStdout(const std::string &msg, sockaddr *address);
void handleConnection(int socketDescriptor, sockaddr acceptedAddress);

static addrinfo *addressInfo{nullptr};

int main(int argc, char *argv[])
{
    //[[maybe_unused]]
    StaticLogger::initializeInstance(globalLogHandler);

    /* getopt_long stores the option index here. */
    int optionIndex{0};
    opterr = 0;

    while (true) {
        auto currentOption = getopt_long(argc, argv, "hvp:n:", long_options, &optionIndex);
        /* Detect the end of the options. */
        if (currentOption == -1) {
            break;
        }
        switch (currentOption) {
            case 0:
                /* If this option set a flag, do nothing else now. */
                if (long_options[optionIndex].flag != nullptr) {
                    break;
                }
                std::cout << "Option " << long_options[optionIndex].name;
                if (optarg) {
                    std::cout << " with arg " << optarg;
                }
                std::cout << std::endl;
                break;
            case 'h':
                displayHelp();
                exit(EXIT_SUCCESS);
            case 'v':
                displayVersion();
                exit(EXIT_SUCCESS);
            case 'p':
                portNumber = std::stoi(optarg);
                break;
            case 'n':
                hostName = optarg;
                break;
            default:
                LOG_WARN() << TStringFormat(R"(Unknown option "{0}", skipping)", long_options[optionIndex].name);
        }
    }
    displayVersion();
    LOG_INFO() << TStringFormat("Using log file {0}", ApplicationUtilities::getLogFilePath());


    if ( (portNumber != -1) && (portNumber < MINIMUM_PORT_NUMBER) ) {
        LOG_FATAL("") << TStringFormat("Port number may not be less than {0} ({1} < 0)", MINIMUM_PORT_NUMBER, portNumber);
    }
    if (portNumber == -1) {
        for (int i = 1; i < argc; i++) {
            if ( (strlen(argv[i]) > 0) && (argv[i][0] != '-') && !(looksLikeIP(argv[i]))) {
                portNumber = std::stoi(argv[i]);
            }
        }
        if (portNumber == -1) {
            LOG_FATAL("") << "Please specify a port number to bind to";
        }
    }

    if (hostName.empty()) {
        for (int i = 1; i < argc; i++) {
            if ( (strlen(argv[i]) > 0) && (argv[i][0] != '-') && (looksLikeIP(argv[i]))) {
                hostName = argv[i];
            }
        }
        if (hostName.empty()) {
            hostName = getDefaultHostName();
        }
    }
    if ( (portNumber != -1) && (portNumber < MINIMUM_PORT_NUMBER) ) {
        LOG_FATAL("") << TStringFormat("Port number may not be less than {0} ({1} < 0)", MINIMUM_PORT_NUMBER, portNumber);
    }
    LOG_INFO() << TStringFormat("Using host name {0}", hostName);
    LOG_INFO() << TStringFormat("Using port number {0}", portNumber);
    addrinfo hints{};
    memset(reinterpret_cast<void *>(&hints), 0, sizeof(addrinfo));
    hints.ai_family = AF_UNSPEC; //IPV4 or IPV6
    hints.ai_socktype = SOCK_STREAM; //TCP
    hints.ai_flags = 0; //Let me specify IP Address
    installSignalHandlers(signalHandler);
    auto returnStatus = getaddrinfo(
            hostName.c_str(),
            toStdString(portNumber).c_str(), //Service (HTTP, port, etc)
            &hints, //Use the hints specified above
            &addressInfo //Pointer to linked list to be filled in by getaddrinfo 
    );
    //or hints.ai_flags = 0
    //auto returnStatus = getaddrinfo(
    //      "127.0.0.1",
    //      port,
    //      &hints,
    //      &addressInfo
    //);
    if (returnStatus != 0) {
        std::cout << "getaddrinfo(const char *, const char *, constr addrinfo *, addrinfo **): error code " << returnStatus << " (" << gai_strerror(returnStatus) << ")" << std::endl;
        exitApplication(EXIT_FAILURE);
    }

    /*
    //iterate the AddressInfo linked list given by getaddrinfo
    //to find valid AddressInfo
    AddressInfo *addrInfoPtr{nullptr}   
    char ipstr[INET6_ADDRSTRLEN];
    for(AddressInfo *addrInfoPtr = addressInfo; addrInfoPtr != nullptr; addrInfoPtr = addrInfoPtr->ai_next) {
        void *addr{nullptr};
        char ipver[10];
        // get the pointer to the address itself,
        // different fields in IPv4 and IPv6:
        if (addrInfoPtr->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = reinterpret_cast<struct sockaddr_in *>(addrInfoPtr->ai_addr);
            addr = &(ipv4->sin_addr);
            strcpy(ipver, "IPv4");
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = reinterpret_cast<struct sockaddr_in6 *>(addrInfoPtr->ai_addr);
            addr = &(ipv6->sin6_addr);
            strcpy(ipver, "IPv6");
        }

        // convert the IP to a string and print it:
        inet_ntop(addrInfoPtr->ai_family, addr, ipstr, sizeof(ipstr));
    } 
    */
    auto socketDescriptor = socket(addressInfo->ai_family, addressInfo->ai_socktype, addressInfo->ai_protocol);
    if (socketDescriptor == -1) {
        std::cout << "socket(int, int, int): error code " << errno << " (" << strerror(errno) << ")" << std::endl;
        exitApplication(EXIT_FAILURE);
    } else {
        socketFileDescriptor = &socketDescriptor;
    }
    
    //For a client, bind is only important is we want to choose the local port to bind to
    //If a socket is not bound before connect(), the kernel will choose a random one
    //However, for a server, bind MUST be called before calling listen()
    auto bindResult = bind(socketDescriptor, addressInfo->ai_addr, addressInfo->ai_addrlen);
    if (bindResult == -1) {
        std::cout << "bind(int, sockaddr*, int) : error code " << errno << " (" << strerror(errno) << ")" << std::endl;
        exitApplication(EXIT_FAILURE);
    }
    
    int acceptReuse{1};
    auto reuseSocketResult = setsockopt(socketDescriptor, SOL_SOCKET, SO_REUSEADDR, &acceptReuse, sizeof(decltype(acceptReuse)));
    if (reuseSocketResult == -1) {
        std::cout << "setsockopt(int, int, int, const void *, socklen_t): error code " << errno << " (" << strerror(errno) << ")" << std::endl;
        exitApplication(EXIT_FAILURE);
    }
    

    /*
    //server does not need to connect, but client would use:
    auto connectResult = connect(socketDescriptor, addressInfo->ai_addr, addressInfo->ai_addrlen);
    if (connectResult == -1) {
        std::cout << "connect(int, const sockaddr *addr, socklen_t): error code " << errno << " (" << strerror(errno) << ")" << std::endl;
        exitApplication(EXIT_FAILURE);    
    }
    */

    auto listenResult = listen(socketDescriptor, MAX_INCOMING_CONNECTIONS);
    if (listenResult == -1) {
        std::cout << "listen(int, int): error code " << errno << " (" << strerror(errno) << ")" << std::endl;
        exitApplication(EXIT_FAILURE);
    }

    sockaddr acceptedAddress{};
    socklen_t acceptedAddressSize{0};
    while (true) {
        acceptedAddress = {};
        acceptedAddressSize = sizeof(acceptedAddress);
        auto acceptResult = accept(socketDescriptor, &acceptedAddress, &acceptedAddressSize);
        if (acceptResult == -1) {
            printToStdout(TStringFormat("accept(int, sockaddr *, size_t *): error code {0} ({1})", errno, strerror(errno)));
            exitApplication(EXIT_FAILURE);
        }
        connections.emplace(socketDescriptor, std::async(std::launch::async, handleConnection, acceptResult, acceptedAddress));

    }
}


bool looksLikeIP(const char *str)
{
    static const std::regex ipv4Regex{"((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])"};
    static const std::regex ipv6Regex{"(([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:)|fe80:(:[0-9a-fA-F]{0,4}){0,4}%[0-9a-zA-Z]{1,}|::(ffff(:0{1,4}){0,1}:){0,1}((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])|([0-9a-fA-F]{1,4}:){1,4}:((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9]))"};
    return (std::regex_match(str, ipv4Regex) || std::regex_match(str, ipv6Regex));
}

void handleConnection(int socketDescriptor, sockaddr addressStorage)
{
    printAddressMessageToStdout("Incoming connection", &addressStorage);
    //Set timeout
    auto tv = toTimeVal(RECEIVE_TIMEOUT);
    auto readTimeoutResult = setsockopt(socketDescriptor, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&tv), sizeof(struct timeval));
    if (readTimeoutResult == -1) {
        printToStdout(TStringFormat("setsockopt(int, int, int, const void *, int): error code {0} ({1})", errno, strerror(errno)));
        exitApplication(EXIT_FAILURE);
    }
    
    char buffer[BUFFER_MAX];
    while (true) {
        memset(buffer, '\0', BUFFER_MAX);
        auto receiveResult = recv(socketDescriptor, buffer, BUFFER_MAX - 1, 0); //no flags
        if (receiveResult == -1) {
            if (errno != EAGAIN) {
                printToStdout(TStringFormat("recv(int, void *, size_t, int): error code {0} ({1})", errno, strerror(errno)));
                exitApplication(EXIT_FAILURE);
            }
        } else if (strlen(buffer) != 0) {
            std::string receivedString{"Message received: \"" + stripLineEnding(buffer) + "\""};
            printAddressMessageToStdout(TStringFormat("Rx << {0}", stripLineEnding(buffer)), &addressStorage);
            receivedString += LINE_ENDING;
            unsigned sentBytes{0};
            //Make sure all bytes are sent
            while (sentBytes < receivedString.length()) {
                auto toSend = receivedString.substr(sentBytes);
                auto sendResult = send(socketDescriptor, toSend.c_str(), toSend.length(), 0);
                if (sendResult == -1) {
                    printToStdout(TStringFormat("send(int, const void *, int, int): error code {0} ({1})", errno, strerror(errno)));
                    exitApplication(EXIT_FAILURE);
                }
                sentBytes += sendResult;
                printAddressMessageToStdout(TStringFormat("Tx >> {0}", stripLineEnding(receivedString)), &addressStorage);
            }
        } else if (receiveResult == 0) {
            printAddressMessageToStdout("Connection closed", &addressStorage);
            closeConnection(socketDescriptor);
            return;
        }
    }
}

void closeConnection(int socketDescriptor)
{
    auto foundPosition = connections.find(socketDescriptor);
    if (foundPosition != connections.end()) {
        close(foundPosition->first);
        connections.erase(foundPosition);
    }
}

struct timeval toTimeVal(uint32_t totalTimeout)
{
    timeval tv{};
    tv.tv_sec = totalTimeout / 1000;
    tv.tv_usec = (totalTimeout % 1000) * 1000;
    return tv;
}


std::string sockaddrToString(sockaddr *address)
{
    std::stringstream returnString{""};
    char host[NI_MAXHOST];
    char port[NI_MAXSERV];
    memset(host, '\0', NI_MAXHOST);
    memset(port, '\0', NI_MAXSERV);

    getnameinfo(address, sizeof(*address), host, sizeof(host), port, sizeof(port), NI_NUMERICHOST);
    returnString << '[' << host << ':' << port << ']';
    return returnString.str();
}

void printAddressMessageToStdout(const std::string &msg, sockaddr *address)
{
    printToStdout(msg + " - " + sockaddrToString(address));
}

void printToStdout(const std::string &msg)
{
    std::lock_guard<std::mutex> coutLock{coutMutex};
    (void)coutLock;
    std::cout << msg << std::endl;
}

void exitApplication(int exitCode) 
{
    if (socketFileDescriptor) {
        close(*socketFileDescriptor);
    }
    freeaddrinfo(addressInfo);
    exit(exitCode);
}

void signalHandler(int signal)
{
    if ( (signal == SIGUSR1) || (signal == SIGUSR2) ) {
        return;
    }
    LOG_INFO() << TStringFormat("Signal received: {0} ({1})", signal, strsignal(signal));
    exitApplication(EXIT_FAILURE);
}


void displayVersion()
{
    using namespace ApplicationUtilities;
    LOG_INFO() << TStringFormat("{0}, v{1}.{2}.{3}", PROGRAM_NAME, SOFTWARE_MAJOR_VERSION, SOFTWARE_MINOR_VERSION, SOFTWARE_PATCH_VERSION);
    LOG_INFO() << TStringFormat("Written by {0}", AUTHOR_NAME);
    LOG_INFO() << TStringFormat("Built with {0} v{1}.{2}.{3}, {4}", COMPILER_NAME, COMPILER_MAJOR_VERSION, COMPILER_MINOR_VERSION, COMPILER_PATCH_VERSION, __DATE__);
}


void displayHelp()
{
    using namespace ApplicationUtilities;
    std::cout << TStringFormat("Usage: {0} Option [=value]", PROGRAM_NAME) << std::endl;
    std::cout << "Options: " << std::endl;
    std::cout << "    -h, --h, -help, --help: Display this help text" << std::endl;
    std::cout << "    -v, --v, -version, --version: Display the version" << std::endl;
    std::cout << "    -e, --e, -verbose, --verbose: Enable verbose output" << std::endl;
}

template <typename StringType, typename FileStringType>
void logToFile(const StringType &str, const FileStringType &filePath)
{
    using namespace ApplicationUtilities;
    std::ofstream writeToFile{};
    writeToFile.open(filePath.c_str(), std::ios::app);
    if (!writeToFile.is_open()) {
        throw std::runtime_error(TStringFormat(R"(Failed to log data "{0}" to file "{1}" (could not open file))", str, filePath));
    } else {
        writeToFile << str << std::endl;
        if (!writeToFile.good()) {
            throw std::runtime_error(TStringFormat(
                    R"(Failed to log data "{0}" to file "{1}" (file was opened, but not writable, permission problem?))", str, filePath));
        }
        writeToFile.close();
    }

}

void globalLogHandler(LogLevel logLevel, LogContext logContext, const std::string &str)
{
    using namespace ApplicationUtilities;
    std::string logPrefix{""};
    auto *outputStream = &std::cout;
    switch (logLevel) {
        case LogLevel::Debug:
            if (!verboseLogging) {
                return;
            }
            logPrefix = "{  Debug }: ";
            outputStream = &std::clog;
            break;
        case LogLevel::Info:
            logPrefix = "{  Info  }: ";
            outputStream = &std::cout;
            break;
        case LogLevel::Warn:
            logPrefix = "{  Warn  }: ";
            outputStream = &std::cout;
            break;
        case LogLevel::Fatal:
            logPrefix = "{  Fatal }: ";
            outputStream = &std::cerr;
    }
    std::string logMessage{""};
    std::string coreLogMessage{str};
    if (coreLogMessage.find('\"') == 0) {
        coreLogMessage = coreLogMessage.substr(1);
    }
    if (coreLogMessage.find_last_of('\"') == coreLogMessage.length() - 1) {
        coreLogMessage = coreLogMessage.substr(0, coreLogMessage.length() - 1);
    }
    std::string logTime{currentTime()};
    std::replace(logTime.begin(), logTime.end(), '-', ':');
    //logTime.replace(logTime.begin(), logTime.end(), '-', ':');
    if (logLevel == LogLevel::Fatal) {
        logMessage = TStringFormat("[{0}] - {1} {2} ({3}:{4}, {5})", logTime, logPrefix, coreLogMessage, logContext.fileName, logContext.sourceFileLine, logContext.functionName);
    } else {
        logMessage = TStringFormat("[{0}] - {1} {2}", logTime, logPrefix, coreLogMessage);
    }


    bool addLineEnding{true};
    static const std::forward_list<const char *> LINE_ENDINGS{"\r\n", "\r", "\n", "\n\r"};
    for (const auto &it : LINE_ENDINGS) {
        if (endsWith(logMessage, it)) {
            addLineEnding = false;
        }
    }
    if (addLineEnding) {
        logMessage.append("\n");
    }
    if (outputStream) {
        *outputStream << logMessage;
    }
    if (logLevel != LogLevel::Fatal) {
        logToFile(logMessage, ApplicationUtilities::getLogFilePath());
    }
    outputStream->flush();
    if (logLevel == LogLevel::Fatal) {
        abort();
    }
}

using StringPair = std::pair<std::string, std::string>;

bool startsWith(const std::string &str, const std::string &beginning)
{
    if (beginning.length() > str.length()) {
        return false;
    }
    return std::equal(beginning.begin(), beginning.end(), str.begin());
}

std::vector<StringPair> getLocalIP()
{
    std::vector<StringPair> returnPair{};
    ifaddrs *addressInfo{nullptr};
    auto getAddressInfoResult = getifaddrs(&addressInfo);
    if (getAddressInfoResult == -1) {
        throw std::runtime_error("getifaddrs(sockaddr *): error code " + std::to_string(errno) + " (" + strerror(errno) + ')');
    }
    auto tempAddress = addressInfo;
    while (tempAddress) {
        if (tempAddress->ifa_addr) {
            if (tempAddress->ifa_addr->sa_family == AF_INET) {
                auto pAddr = reinterpret_cast<sockaddr_in *>(tempAddress->ifa_addr);
                char nameBuffer[INET_ADDRSTRLEN];
                memset(nameBuffer, '\0', INET_ADDRSTRLEN);
                inet_ntop(AF_INET, &(pAddr->sin_addr), nameBuffer, INET_ADDRSTRLEN);
                returnPair.emplace_back(tempAddress->ifa_name, nameBuffer);
            } else if (tempAddress->ifa_addr->sa_family == AF_INET6) {
                auto pAddr = reinterpret_cast<sockaddr_in6 *>(tempAddress->ifa_addr);
                char nameBuffer[INET6_ADDRSTRLEN];
                memset(nameBuffer, '\0', INET6_ADDRSTRLEN);
                inet_ntop(AF_INET6, &(pAddr->sin6_addr), nameBuffer, INET6_ADDRSTRLEN);
                returnPair.emplace_back(tempAddress->ifa_name, nameBuffer);
            }
        }
        tempAddress = tempAddress->ifa_next;
    }

    freeifaddrs(addressInfo);
    return returnPair;
} 

std::string getDefaultHostName()
{
    for (const auto &it : getLocalIP()) {
        if (startsWith(it.second, "192")) {
            return it.second;
        }
    }
    return "127.0.0.1";
}
