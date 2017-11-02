#ifndef PROJECTTEMPLATE_STATICLOGGER_H
#define PROJECTTEMPLATE_STATICLOGGER_H
#include <sstream>

#include <string>
#include <functional>
#include <ctime>
#include <iomanip>


enum class LogLevel {
    Debug,
    Info,
    Warn,
    Fatal
};

struct LogContext
{
    const char *fileName;
    const char *functionName;
    int sourceFileLine;
};

class Logger;
using LogFunction = std::function<void(LogLevel, LogContext, const std::string &)>;

class StaticLogger
{
    friend class Logger;
public:
    StaticLogger(const StaticLogger &) = delete;
    StaticLogger(StaticLogger &&) = delete;
    StaticLogger &operator=(const StaticLogger &) = delete;
    StaticLogger &operator=(StaticLogger &&) = delete;

    static void initializeInstance();
    static LogFunction installLogHandler(const LogFunction &logHandler);

private:
    std::function<void(LogLevel, LogContext, const std::string &)> m_logHandler;

    StaticLogger();
    static void log(const Logger &logger);

};

extern StaticLogger *staticLogger;


class Logger
{
public:
    inline ~Logger() {
        StaticLogger::log(*this);
    }

    template <typename T>
    static inline Logger createInstance(LogLevel logLevel, const char *fileName, int sourceFileLine, const char *functionName, const T &t) {
        return Logger{logLevel, fileName, sourceFileLine, functionName, t};
    }

    static inline Logger createInstance(LogLevel logLevel, const char *fileName, int sourceFileLine, const char *functionName) {
        return Logger{logLevel, fileName, sourceFileLine, functionName};
    }


    template <typename T>
    inline Logger &operator<<(const T &t) {
        this->m_logMessage += toStdString(t);
        return *this;
    }

    inline const LogLevel &logLevel() const { return this->m_logLevel; }
    inline const std::string &logMessage() const { return this->m_logMessage; }
    inline const LogContext &logContext() const {return this->m_logContext; }

private:
    LogLevel m_logLevel;
    std::string m_logMessage;
    LogContext m_logContext;

    template <typename T> static inline std::string toStdString(T t) { return dynamic_cast<std::ostringstream &>(std::ostringstream{} << t).str(); }

    template <typename T>
    inline Logger(LogLevel logLevel, const char *fileName, int sourceFileLine, const char *functionName, const T &t) :
        m_logLevel{logLevel},
        m_logMessage{toStdString(t)},
        m_logContext{} {
        this->m_logContext.fileName = fileName;
        this->m_logContext.sourceFileLine = sourceFileLine;
        this->m_logContext.functionName = functionName;
    }

    inline Logger(LogLevel logLevel, const char *fileName, int sourceFileLine, const char *functionName) :
            m_logLevel{logLevel},
            m_logMessage{""},
            m_logContext{} {
        this->m_logContext.fileName = fileName;
        this->m_logContext.sourceFileLine = sourceFileLine;
        this->m_logContext.functionName = functionName;
    }


};



#endif //PROJECTTEMPLATE_STATICLOGGER_H
