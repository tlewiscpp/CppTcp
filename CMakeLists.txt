cmake_minimum_required(VERSION 3.8)
set(PROJECT_NAME CppTcp)
project(${PROJECT_NAME} CXX)
set(CMAKE_CXX_STANDARD 17)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set (SOURCE_ROOT .)

set(${PROJECT_NAME}_SOURCE_FILES
        ${SOURCE_ROOT}/TcpServer.cpp
        ${SOURCE_ROOT}/ApplicationUtilities.cpp
        ${SOURCE_ROOT}/StaticLogger.cpp)

set (${PROJECT_NAME}_HEADER_FILES
        ${SOURCE_ROOT}/ApplicationUtilities.h
        ${SOURCE_ROOT}/StaticLogger.h
        ${SOURCE_ROOT}/GlobalDefinitions.h)

add_executable(${PROJECT_NAME}
        ${${PROJECT_NAME}_SOURCE_FILES}
        ${${PROJECT_NAME}_HEADER_FILES})