//
// Created by pinguinsan on 11/3/17.
//

#include "TcpClient.h"

#include <unistd.h>
#include <cstring>

using namespace CppSerialPort;

#define MINIMUM_PORT_NUMBER 1024
#define BUFFER_MAX 8192

TcpClient::TcpClient(const std::string &hostName, uint16_t portNumber)
{
    if (portNumber < MINIMUM_PORT_NUMBER) {
        throw std::runtime_error("portNumber cannot be less than minimum value (" + toStdString(portNumber) + " < " + toStdString(MINIMUM_PORT_NUMBER) + ")");
    }
    this->m_hostName = hostName;
    this->m_portNumber = portNumber;
    this->m_socketDescriptor = -1;
}

TcpClient::~TcpClient()
{
    if (this->isConnected()) {
        this->disconnect();
    }
}

void TcpClient::connect(const std::string &hostName, uint16_t portNumber)
{
    if (this->isConnected()) {
        throw std::runtime_error("Cannot connect to new host when already connected (call disconnect() first");
    }
    this->m_hostName = hostName;
    this->m_portNumber = portNumber;
    this->connect();
}

void TcpClient::connect()
{
    if (this->isConnected()) {
        throw std::runtime_error("Cannot connect to new host when already connected (call disconnect() first");
    }
    addrinfo *addressInfo{nullptr};
    addrinfo hints{};
    memset(reinterpret_cast<void *>(&hints), 0, sizeof(addrinfo));
    hints.ai_family = AF_UNSPEC; //IPV4 or IPV6
    hints.ai_socktype = SOCK_STREAM; //TCP
    auto returnStatus = getaddrinfo(
            this->m_hostName.c_str(), //IP Address or hostname
            toStdString(this->m_portNumber).c_str(), //Service (HTTP, port, etc)
            &hints, //Use the hints specified above
            &addressInfo //Pointer to linked list to be filled in by getaddrinfo
    );
    if (returnStatus != 0) {
        freeaddrinfo(addressInfo);
        throw std::runtime_error("getaddrinfo(const char *, const char *, constr addrinfo *, addrinfo **): error code " + toStdString(returnStatus) + " (" + gai_strerror(returnStatus) + ")");
    }
    auto socketDescriptor = socket(addressInfo->ai_family, addressInfo->ai_socktype, addressInfo->ai_protocol);
    if (socketDescriptor == -1) {
        freeaddrinfo(addressInfo);
        throw std::runtime_error("socket(int, int, int): error code " + toStdString(errno) + " (" + strerror(errno) + ")");
    }
    this->m_socketDescriptor = socketDescriptor;

    int acceptReuse{1};
    auto reuseSocketResult = setsockopt(socketDescriptor, SOL_SOCKET, SO_REUSEADDR, &acceptReuse, sizeof(decltype(acceptReuse)));
    if (reuseSocketResult == -1) {
        freeaddrinfo(addressInfo);
        throw std::runtime_error("setsockopt(int, int, int, const void *, socklen_t): error code " + toStdString(errno) + " (" + strerror(errno) + ")");
    }

    auto connectResult = ::connect(this->m_socketDescriptor, addressInfo->ai_addr, addressInfo->ai_addrlen);
    if (connectResult == -1) {
        freeaddrinfo(addressInfo);
        throw std::runtime_error("connect(int, const sockaddr *addr, socklen_t): error code " + toStdString(errno) +  " (" + strerror(errno) + ")");
    }
    freeaddrinfo(addressInfo);
    this->m_readBuffer.clear();
}

bool TcpClient::disconnect()
{
    close(this->m_socketDescriptor);
    this->m_socketDescriptor = -1;
    this->m_readTimeoutSet = false;
}

bool TcpClient::isConnected() const
{
    return this->m_socketDescriptor != -1;
}

int TcpClient::read()
{

    if (!this->m_readBuffer.empty()) {
        char returnValue{this->m_readBuffer.front()};
        this->m_readBuffer = this->m_readBuffer.substr(1);
        return static_cast<int>(returnValue);
    }

    if (!this->m_readTimeoutSet) {
        auto tv = toTimeVal(static_cast<uint32_t>(this->readTimeout()));
        auto readTimeoutResult = setsockopt(this->m_socketDescriptor, SOL_SOCKET, SO_RCVTIMEO,
                                            reinterpret_cast<const char *>(&tv), sizeof(struct timeval));
        if (readTimeoutResult == -1) {
            throw std::runtime_error("setsockopt(int, int, int, const void *, int): error code " + toStdString(errno) + " (" + strerror(errno) + ")");
        }
        this->m_readTimeoutSet = true;
    }

    char buffer[BUFFER_MAX];
    memset(buffer, '\0', BUFFER_MAX);
    auto receiveResult = recv(this->m_socketDescriptor, buffer, BUFFER_MAX - 1, 0); //no flags
    if (receiveResult == -1) {
        if (errno != EAGAIN) {
            throw std::runtime_error("recv(int, void *, size_t, int): error code " + toStdString(errno) + " (" + strerror(errno) + ")");
        } else {
            return 0;
        }
    } else if (strlen(buffer) != 0) {
        this->m_readBuffer += std::string{buffer};
        return this->m_readBuffer[0];
    } else if (receiveResult == 0) {
        this->disconnect();
        throw std::runtime_error("Server hung up unexpectedly");
    }
}

ssize_t TcpClient::write(int i)
{
    if (!this->isConnected()) {
        throw std::runtime_error("Cannot write on closed socket (call connect first)");
    }
    send(this->m_socketDescriptor, &i, 1, 0);
}

ssize_t TcpClient::writeLine(const std::string &str)
{
    if (!this->isConnected()) {
        throw std::runtime_error("Cannot write on closed socket (call connect first)");
    }
    unsigned sentBytes{0};
    //Make sure all bytes are sent
    while (sentBytes < str.length()) {
        std::string toSend{str.substr(sentBytes)};
        auto sendResult = send(this->m_socketDescriptor, toSend.c_str(), toSend.length(), 0);
        if (sendResult == -1) {
            throw std::runtime_error("send(int, const void *, int, int): error code " + toStdString(errno) + " (" + strerror(errno) + ")");
        }
        sentBytes += sendResult;
    }
}

std::string TcpClient::portName() const
{
    return '[' + this->m_hostName + ':' + toStdString(this->m_portNumber) + ']';
}

bool TcpClient::isOpen() const
{
    return this->isConnected();
}

void TcpClient::openPort()
{
    if (!this->isConnected()) {
        this->connect();
    }
}

void TcpClient::closePort()
{
    if (this->isConnected()) {
        this->disconnect();
    }
}

void TcpClient::flushRx()
{

}

void TcpClient::flushTx()
{

}

void TcpClient::putBack(int c)
{
    this->m_readBuffer.insert(this->m_readBuffer.begin(), static_cast<char>(c));
}

timeval TcpClient::toTimeVal(uint32_t totalTimeout)
{
    timeval tv{};
    tv.tv_sec = totalTimeout / 1000;
    tv.tv_usec = (totalTimeout % 1000) * 1000;
    return tv;
}

void TcpClient::setPortNumber(uint16_t portNumber) {
    if (this->isConnected()) {
        throw std::runtime_error("Cannot set port number when already connected (call disconnect() first");
    }
    this->m_portNumber = portNumber;
}

void TcpClient::setHostName(const std::string &hostName) {
    if (this->isConnected()) {
        throw std::runtime_error("Cannot set host name when already connected (call disconnect() first");
    }
    this->m_hostName = hostName;
}

uint16_t TcpClient::portNumber() const {
    return this->m_portNumber;
}

std::string TcpClient::hostName() const {
    return this->m_hostName;
}
