cmake_minimum_required(VERSION 3.1)
set(PROJECT_NAME CppTcp)
project(${PROJECT_NAME} CXX)
set(CMAKE_CXX_STANDARD 17)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(SERVER_PROJECT ${PROJECT_NAME}Server)
set(CLIENT_PROJECT ${PROJECT_NAME}Client)

set (SOURCE_ROOT .)

set(${SERVER_PROJECT}_SOURCE_FILES
        ${SOURCE_ROOT}/CppTcpServer.cpp
        ${SOURCE_ROOT}/ApplicationUtilities.cpp
        ${SOURCE_ROOT}/StaticLogger.cpp)

set (${SERVER_PROJECT}_HEADER_FILES
        ${SOURCE_ROOT}/ApplicationUtilities.h
        ${SOURCE_ROOT}/StaticLogger.h
        ${SOURCE_ROOT}/GlobalDefinitions.h)

set(${CLIENT_PROJECT}_SOURCE_FILES
        ${SOURCE_ROOT}/CppTcpClient.cpp
        ${SOURCE_ROOT}/ApplicationUtilities.cpp
        ${SOURCE_ROOT}/StaticLogger.cpp
        ${SOURCE_ROOT}/TcpClient.cpp
        ${SOURCE_ROOT}/IByteStream.cpp)

set(${CLIENT_PROJECT}_HEADER_FILES
    ${SOURCE_ROOT}/TcpClient.h
    ${SOURCE_ROOT}/IByteStream.h
    ${SOURCE_ROOT}/ApplicationUtilities.h
    ${SOURCE_ROOT}/StaticLogger.h
    ${SOURCE_ROOT}/GlobalDefinitions.h)

add_executable(${SERVER_PROJECT}
        ${${SERVER_PROJECT}_SOURCE_FILES}
        ${${SERVER_PROJECT}_HEADER_FILES})

add_executable(${CLIENT_PROJECT}
        ${${CLIENT_PROJECT}_SOURCE_FILES}
        ${${CLIENT_PROJECT}_HEADER_FILES})


target_link_libraries(${SERVER_PROJECT} pthread)
target_link_libraries(${CLIENT_PROJECT} pthread)
