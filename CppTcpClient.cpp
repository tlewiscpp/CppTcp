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
#include <netdb.h>

#include <unistd.h>

#include "ApplicationUtilities.h"
#include "GlobalDefinitions.h"
#include "TcpClient.h"

#include <getopt.h>
#include <arpa/inet.h>

using namespace ApplicationUtilities;

static int verboseLogging{false};
void globalLogHandler(LogLevel logLevel, LogContext logContext, const std::string &str);
std::map<int, std::future<void>> connections;
void closeConnection(int socketDescriptor);
bool looksLikeIP(const char *str);
std::string stdinTask();

std::string tcpReadTask();

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
static const int constexpr MINIMUM_PORT_NUMBER{1024};
static int portNumber{-1};
static std::string hostName{""};
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

void printToStdout(const std::string &msg);
void printAddressMessageToStdout(const std::string &msg);

static std::shared_ptr<TcpClient> tcpClient{nullptr};

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
                LOG_WARN("") << TStringFormat(R"(Unknown option "{0}", skipping)", long_options[optionIndex].name);
        }
    }
    displayVersion();
    LOG_INFO("") << TStringFormat("Using log file {0}", ApplicationUtilities::getLogFilePath());


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
            LOG_FATAL("") << "Please specify a port number to connect to";
        }
    }

    if (hostName.empty()) {
        for (int i = 1; i < argc; i++) {
            if ( (strlen(argv[i]) > 0) && (argv[i][0] != '-') && (looksLikeIP(argv[i]))) {
                hostName = argv[i];
            }
        }
        if (hostName.empty()) {
            LOG_FATAL("") << "Please specify a host name to connect to";
        }
    }
    LOG_INFO("") << TStringFormat("Using host name {0}", hostName);
    LOG_INFO("") << TStringFormat("Using port number {0}", portNumber);
    LOG_INFO("") << "Enter message to send";
    tcpClient = std::make_shared<TcpClient>(hostName, static_cast<uint16_t>(portNumber));
    tcpClient->setLineEnding(LINE_ENDING);
    tcpClient->connect();

    using StringFuture = std::future<std::string>;
    StringFuture tcpFuture{std::async(std::launch::async, tcpReadTask)};
    StringFuture stdinFuture{std::async(std::launch::async, stdinTask)};
    while (true) {
        if (tcpFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            printToStdout("Rx << " + tcpFuture.get());
            tcpFuture = std::async(std::launch::async, tcpReadTask);
        }
        if (stdinFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            std::string toSend{stdinFuture.get()};
            tcpClient->writeLine(toSend);
            stdinFuture = std::async(std::launch::async, stdinTask);
        }
    }
}

void printAddressMessageToStdout(const std::string &msg)
{
    printToStdout(msg + " - [" + hostName + ':' + toStdString(portNumber) + ']');
}

bool looksLikeIP(const char *str)
{
    static const std::regex ipv4Regex{"((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])"};
    static const std::regex ipv6Regex{"(([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:)|fe80:(:[0-9a-fA-F]{0,4}){0,4}%[0-9a-zA-Z]{1,}|::(ffff(:0{1,4}){0,1}:){0,1}((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])|([0-9a-fA-F]{1,4}:){1,4}:((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9]))"};
    return (std::regex_match(str, ipv4Regex) || std::regex_match(str, ipv6Regex));
}

void printToStdout(const std::string &msg)
{
    std::lock_guard<std::mutex> coutLock{coutMutex};
    (void)coutLock;
    std::cout << msg << std::endl;
}

void exitApplication(int exitCode) 
{
    exit(exitCode);
}

std::string stdinTask() {
    std::string returnString{""};
    std::getline(std::cin, returnString);
    return returnString;
}

std::string tcpReadTask() {
    std::string returnString{""};
    bool timeout{false};
    while (true) {
        returnString = tcpClient->readLine(&timeout);
        if ( (!returnString.empty()) && (!timeout) ) {
            return returnString;
        }
    }
}

void signalHandler(int signal)
{
    if ( (signal == SIGUSR1) || (signal == SIGUSR2) ) {
        return;
    }
    LOG_INFO("") << TStringFormat("Signal received: {0} ({1})", signal, strsignal(signal));
    exitApplication(EXIT_FAILURE);
}


void displayVersion()
{
    using namespace ApplicationUtilities;
    LOG_INFO("") << TStringFormat("{0}, v{1}.{2}.{3}", PROGRAM_NAME, SOFTWARE_MAJOR_VERSION, SOFTWARE_MINOR_VERSION, SOFTWARE_PATCH_VERSION);
    LOG_INFO("") << TStringFormat("Written by {0}", AUTHOR_NAME);
    LOG_INFO("") << TStringFormat("Built with {0} v{1}.{2}.{3}, {4}", COMPILER_NAME, COMPILER_MAJOR_VERSION, COMPILER_MINOR_VERSION, COMPILER_PATCH_VERSION, __DATE__);
}


void displayHelp()
{
    using namespace ApplicationUtilities;
    std::cout << TStringFormat("Usage: {0} Option [=value]", PROGRAM_NAME) << std::endl;
    std::cout << "Options: " << std::endl;
    std::cout << "    -h, --h, -help, --help: Display this help text" << std::endl;
    std::cout << "    -v, --v, -version, --version: Display the version" << std::endl;
    std::cout << "    -e, --e, -verbose, --verbose: Enable verbose output" << std::endl;
    std::cout << "    -p, --p, -port, --port: Specify port number" << std::endl;
    std::cout << "    -n, --n, -name, --name: Specify host name" << std::endl;

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