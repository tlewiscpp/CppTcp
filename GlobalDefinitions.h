//
// Created by pinguinsan on 11/2/17.
//

#ifndef PROJECTTEMPLATE_GLOBALDEFINITIONS_H
#define PROJECTTEMPLATE_GLOBALDEFINITIONS_H
#include "StaticLogger.h"

const char * const PROGRAM_NAME{"CppTcp"};
const char * const COMPANY_NAME{"Tyler Lewis"};
const char * const PROGRAM_LONG_NAME{"CppTcp"};
const char * const PROGRAM_DESCRIPTION{"Tcp server in C++"};
const char * const REMOTE_URL{""};
const char * const AUTHOR_NAME{"Tyler Lewis"};
const int SOFTWARE_MAJOR_VERSION{0};
const int SOFTWARE_MINOR_VERSION{1};
const int SOFTWARE_PATCH_VERSION{0};

#if defined(__GNUC__)
const char * const COMPILER_NAME{"g++"};
const int COMPILER_MAJOR_VERSION{__GNUC__};
const int COMPILER_MINOR_VERSION{__GNUC_MINOR__};
const int COMPILER_PATCH_VERSION{__GNUC_PATCHLEVEL__};
#elif defined(_MSC_VER)
const char *COMPILER_NAME{"msvc"};
        const int COMPILER_MAJOR_VERSION{_MSC_VER};
        const int COMPILER_MINOR_VERSION{0};
        const int COMPILER_PATCH_VERSION{0};
    #else
        const char *COMPILER_NAME{"unknown"};
        const int COMPILER_MAJOR_VERSION{0};
        const int COMPILER_MINOR_VERSION{0};
        const int COMPILER_PATCH_VERSION{0};
    #endif


#ifndef LOG_DEBUG
#    define LOG_DEBUG(x) Logger::createInstance(LogLevel::Debug, __FILE__, __LINE__, __func__, x)
#endif //LOG_FATAL
#ifndef LOG_WARN
#    define LOG_WARN(x) Logger::createInstance(LogLevel::Debug, __FILE__, __LINE__, __func__, x)
#endif //LOG_WARN
#ifndef LOG_INFO
#    define LOG_INFO(x) Logger::createInstance(LogLevel::Info, __FILE__, __LINE__, __func__, x)
#endif //LOG_INFO
#ifndef LOG_FATAL
#    define LOG_FATAL(x) Logger::createInstance(LogLevel::Fatal, __FILE__, __LINE__, __func__, x)
#endif //LOG_FATAL

#ifndef STRING_TO_INT
#    if defined(__ANDROID__)
#        define STRING_TO_INT(x) std::atoi(x.c_str())
#    else
#        define STRING_TO_INT(x) std::stoi(x)
#    endif
#endif //STRING_TO_INT

#ifndef INT_TO_STRING
#   define INT_TO_STRING(x) std::to_string(x)
#endif //INT_TO_STRING


#endif //PROJECTTEMPLATE_GLOBALDEFINITIONS_H
