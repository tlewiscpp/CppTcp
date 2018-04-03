#include "ApplicationUtilities.h"
#include "GlobalDefinitions.h"
#include <csignal>
#include <iomanip>
#include <iostream>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <climits>

namespace ApplicationUtilities {

    std::string getTempDirectory() {
    #ifdef _MSC_VER
        //return someBullShit;
            return "/tmp";
    #else
        return TStringFormat("/tmp/{0}", PROGRAM_NAME);
    #endif
    }

    bool looksLikeFilePath(const std::string &str) {
        static const std::regex DIRECTORY_REGEX{R"(^(/)?([^/\0]+(/)?)+$)"};
        return ( ((str.length() == 1) && (str.front() == '.')) || ((std::regex_match(str, DIRECTORY_REGEX)) && startsWith(str, '/')) );
    }

    int getPID() {
#if defined(_WIN32)
#    error "Not yet implemented"
#else
        return getpid();
#endif //defined(_WIN32)
    }

    std::string getLogFilePath() {
        static std::string logFileName{""};

        if (logFileName.length() > 0) {
            return logFileName;
        } else {
            auto createDirectoryResult = createDirectory(getTempDirectory());
            if (createDirectoryResult == -1) {
                if (errno != EEXIST) {
                    std::cerr << TStringFormat("Could not create log directory {0}: error code {1} ({2})",
                                                   getTempDirectory(), errno, strerror(errno)) << std::endl;
                    abort();
                }
            }
            return TStringFormat("{0}/{1}_{2}_{3}", getTempDirectory(), PROGRAM_NAME, currentDate, currentTime());
        }
    }

    bool endsWith(const std::string &str, const std::string &ending) {
        return (str.length() < ending.length()) ? false : std::equal(ending.rbegin(), ending.rend(), str.rbegin());
    }

    bool endsWith(const std::string &str, char ending) {
        return ( (!str.empty()) && (str.back() == ending));
    }

    bool startsWith(const std::string &str, const std::string &start) {
        return (str.length() < start.length()) ? false : std::equal(start.begin(), start.end(), str.begin());
    }

    bool startsWith(const std::string &str, char start) {
        return ( (!str.empty()) && (str.front() == start));
    }

void installSignalHandlers(void (*signalHandler)(int)) {
    static struct sigaction signalInterruptHandler;
    signalInterruptHandler.sa_handler = signalHandler;
    sigemptyset(&signalInterruptHandler.sa_mask);
    signalInterruptHandler.sa_flags = 0;
    sigaction(SIGHUP, &signalInterruptHandler, NULL);
    sigaction(SIGINT, &signalInterruptHandler, NULL);
    sigaction(SIGQUIT, &signalInterruptHandler, NULL);
    sigaction(SIGILL, &signalInterruptHandler, NULL);
    sigaction(SIGABRT, &signalInterruptHandler, NULL);
    sigaction(SIGFPE, &signalInterruptHandler, NULL);
    sigaction(SIGPIPE, &signalInterruptHandler, NULL);
    sigaction(SIGALRM, &signalInterruptHandler, NULL);
    sigaction(SIGTERM, &signalInterruptHandler, NULL);
    sigaction(SIGUSR1, &signalInterruptHandler, NULL);
    sigaction(SIGUSR2, &signalInterruptHandler, NULL);
    sigaction(SIGCHLD, &signalInterruptHandler, NULL);
    sigaction(SIGCONT, &signalInterruptHandler, NULL);
    sigaction(SIGTSTP, &signalInterruptHandler, NULL);
    sigaction(SIGTTIN, &signalInterruptHandler, NULL);
    sigaction(SIGTTOU, &signalInterruptHandler, NULL);
}

int split(std::vector<std::string> &output, const std::string &str, char delimiter) {
    int returnSize{0};
    std::stringstream istr{str};
    std::string tempString{""};
    while (std::getline(istr, tempString, delimiter)) {
        output.push_back(tempString);
        returnSize++;
    }
    return returnSize;
}


int split(std::vector<std::string> &output, const std::string &inputString, const std::string &delimiter)  {
    std::string str{inputString};
    int returnSize{0};
    auto splitPosition = str.find(delimiter);
    if (splitPosition == std::string::npos) {
        output.push_back(str);
        return ++returnSize;
    }
    while (splitPosition != std::string::npos) {
        output.push_back(str.substr(0, splitPosition));
        str = str.substr(splitPosition + delimiter.size());
        splitPosition = str.find(delimiter);
        returnSize++;
    }
    return returnSize;
}


std::string TStringFormat(const char *formatting) {
    return std::string{formatting};
}


std::string currentTime() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    return dynamic_cast<std::ostringstream &>(std::ostringstream{} << std::put_time(&tm, "%H-%M-%S")).str();
}

std::string currentDate() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    return dynamic_cast<std::ostringstream &>(std::ostringstream{} << std::put_time(&tm, "%d-%m-%Y")).str();
}


bool fileExists(const std::string &filePath) {
    return ( stat(filePath.c_str(), F_OK) != -1 );
}

bool directoryExists(const std::string &directoryPath) {
    struct stat directoryCheck{};
    return ( (stat(directoryPath.c_str(), &directoryCheck) == 0) && (S_ISDIR(directoryCheck.st_mode)) );
}

std::string getCurrentDirectory() {
    char buffer[PATH_MAX];
    memset(buffer, '\0', PATH_MAX);
    auto result = getcwd(buffer, PATH_MAX);
    if (result == nullptr) {
        std::stringstream message{};
        message << "ERROR: getcwd() returned a nullptr (was a drive removed?)";
        throw std::runtime_error(message.str());
    }
    return std::string{buffer};
}

int removeFile(const std::string &filePath) {
    return std::remove(filePath.c_str());
}

int createDirectory(const std::string &filePath, mode_t permissions) {
    return mkdir(filePath.c_str(), permissions);
}



} //namespace ApplicationUtilities
