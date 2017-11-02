//
// Created by pinguinsan on 11/2/17.
//

#include "StaticLogger.h"

#include <sstream>
#include <iostream>

StaticLogger *staticLogger{nullptr};

namespace {
    void defaultLogFunction(LogLevel logLevel, LogContext logContext, const std::string &str) {
        std::ostream *outputStream{nullptr};
        switch (logLevel) {
            case LogLevel::Debug:
                outputStream = &std::clog;
                break;
            case LogLevel::Info:
                outputStream = &std::cout;
                break;
            case LogLevel::Warn:
                outputStream = &std::cout;
                break;
            case LogLevel ::Fatal:
                outputStream = &std::cerr;
                break;
            default:
                outputStream = &std::cout;
                break;
        }
        *outputStream << str << std::endl;
    }
} //Global namespace

StaticLogger::StaticLogger() :
        m_logHandler{defaultLogFunction}
{ }



void StaticLogger::initializeInstance()
{
    if (staticLogger == nullptr) {
        staticLogger = new StaticLogger{};
    }
}

LogFunction StaticLogger::installLogHandler(const LogFunction &logHandler) {
    auto tempLogger = staticLogger->m_logHandler;
    staticLogger->m_logHandler = logHandler;
    return tempLogger;
}

void StaticLogger::log(const Logger &logger) {
    staticLogger->m_logHandler.operator()(logger.logLevel(), logger.logContext(), logger.logMessage());
}
