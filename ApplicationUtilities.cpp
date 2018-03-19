//
// Created by pinguinsan on 11/2/17.
//

#include "ApplicationUtilities.h"
#include "GlobalDefinitions.h"
#include <csignal>
#include <iomanip>
#include <sys/stat.h>
#include <sys/types.h>
#include <iostream>
#include <getopt.h>

namespace ApplicationUtilities
{

    std::string getTempDirectory() {
    #ifdef _MSC_VER
        //return someBullShit;
            return "/tmp";
    #else
        return TStringFormat("/tmp/{0}", PROGRAM_NAME);
    #endif
    }

    std::string buildShortOptions(option *longOptions, size_t numberOfLongOptions)
    {
        std::string returnString{""};
        for (size_t i = 0; i < numberOfLongOptions; i++) {
            option *currentOption{longOptions + i};
            if (currentOption->val == 0) {
                continue;
            }
            returnString += static_cast<char>(currentOption->val);
            if (currentOption->has_arg == required_argument) {
                returnString += ':';
            }
        }
        return returnString;
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

    bool endsWith(const std::string &str, const std::string &ending)
    {
        if (str.length() < ending.length()) {
            return false;
        }
        return (std::equal(ending.rbegin(), ending.rend(), str.rbegin()));
    }

    bool endsWith(const std::string &str, char ending)
    {
        return ( (str.length() > 0) && (str.back() == ending));
    }

void installSignalHandlers(void (*signalHandler)(int))
{
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

std::vector<std::string> split(std::string str, const std::string &delimiter)
{
    std::vector<std::string> returnContainer;
    auto splitPosition = str.find(delimiter);
    if (splitPosition == std::string::npos) {
        returnContainer.push_back(str);
        return returnContainer;
    }
    while (splitPosition != std::string::npos) {
        returnContainer.push_back(str.substr(0, splitPosition));
        str = str.substr(splitPosition + delimiter.size());
        splitPosition = str.find(delimiter);
    }
    return returnContainer;
}


std::string TStringFormat(const char *formatting)
{
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

int removeFile(const std::string &filePath)
{
    return std::remove(filePath.c_str());
}

int createDirectory(const std::string &filePath, mode_t permissions)
{
    return mkdir(filePath.c_str(), permissions);
}

} //namespace ApplicationUtilities