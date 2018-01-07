#include "TcpClient.h"

#if defined(_WIN32)
#    include "Ws2tcpip.h"
#else
#    include <unistd.h>
#    define INVALID_SOCKET -1
#endif //defined(_WIN32)

#include <cstring>
#include <climits>
#include <iostream>

namespace CppSerialPort {

#define MINIMUM_PORT_NUMBER 1024
#define TCP_CLIENT_BUFFER_MAX 8192

TcpClient::TcpClient(const std::string &hostName, uint16_t portNumber) :
    m_socketDescriptor{INVALID_SOCKET},
    m_hostName{hostName},
    m_portNumber{portNumber},
    m_readBuffer{""}
{
    #if defined(_WIN32)
	WSADATA wsaData{};
	// if this doesn't work
	//WSAData wsaData; // then try this instead
	// MAKEWORD(1,1) for Winsock 1.1, MAKEWORD(2,0) for Winsock 2.0:
	
	auto wsaStartupResult = WSAStartup(MAKEWORD(2, 0), &wsaData);
	if (wsaStartupResult != 0) {
		throw std::runtime_error("CppSerialPort::TcpClient::TcpClient(const std::string &, uint16_t): WSAStartup failed: error code " + toStdString(wsaStartupResult) + " (" + this->getErrorString(wsaStartupResult) + ')');
	}
#endif //defined(_WIN32)
    if (portNumber < MINIMUM_PORT_NUMBER) {
        this->m_portNumber = 0;
        throw std::runtime_error("CppSerialPort::TcpClient::TcpClient(const std::string &, uint16_t): portNumber cannot be less than minimum value (" + toStdString(portNumber) + " < " + toStdString(MINIMUM_PORT_NUMBER) + ')');
    }
	this->setReadTimeout(DEFAULT_READ_TIMEOUT);
}

int TcpClient::getLastError()
{
#if defined(_WIN32)
	return WSAGetLastError();
#else
	return errno;
#endif //defined(_WIN32)
}

std::string TcpClient::getErrorString(int errorCode)
{
	char errorString[PATH_MAX];
	memset(errorString, '\0', PATH_MAX);
#if defined(_WIN32)
	wchar_t *wideErrorString{ nullptr };
	FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPWSTR>(&wideErrorString),
		0,
		nullptr
	);
	size_t converted{ 0 };
	auto conversionResult = wcstombs_s(&converted, errorString, PATH_MAX, wideErrorString, PATH_MAX);
	(void)conversionResult;
	//wcstombs(errorString, wideErrorString, PATH_MAX);
	LocalFree(wideErrorString);
#else
    auto strerrorCode = strerror_r(errorCode, errorString, PATH_MAX);
    if (strerrorCode == nullptr) {
        std::cerr << "strerror_r(int, char *, int): error occurred" << std::endl;
        return "";
    }
#endif //defined(_WIN32)
	return stripLineEndings(errorString);
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
        throw std::runtime_error("CppSerialPort::TcpClient::connect(const std::string &, uint16_t): Cannot connect to new host when already connected (call disconnect() first)");
    }
    this->m_hostName = hostName;
    this->m_portNumber = portNumber;
    this->connect();
}

void TcpClient::connect()
{
    if (this->isConnected()) {
        throw std::runtime_error("CppSerialPort::TcpClient::connect(): Cannot connect to new host when already connected (call disconnect() first)");
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
        throw std::runtime_error("CppSerialPort::TcpClient::connect(): getaddrinfo(const char *, const char *, constr addrinfo *, addrinfo **): error code " + toStdString(returnStatus) + " (" + gai_strerror(returnStatus) + ')');
    }
    auto socketDescriptor = socket(addressInfo->ai_family, addressInfo->ai_socktype, addressInfo->ai_protocol);
    if (socketDescriptor == INVALID_SOCKET) {
        freeaddrinfo(addressInfo);
		auto errorCode = getLastError();
		throw std::runtime_error("CppSerialPort::TcpClient::connect(): socket(int, int, int): error code " + toStdString(errorCode) + " (" + getErrorString(errorCode) + ')');
    }
    this->m_socketDescriptor = socketDescriptor;

#if defined(_WIN32)
	char acceptReuse{ 1 };
#else
	int acceptReuse{ 1 };
#endif //defined(_WIN32)
    auto reuseSocketResult = setsockopt(socketDescriptor, SOL_SOCKET, SO_REUSEADDR, &acceptReuse, sizeof(decltype(acceptReuse)));
    if (reuseSocketResult == -1) {
        freeaddrinfo(addressInfo);
		auto errorCode = getLastError();
        throw std::runtime_error("CppSerialPort::TcpClient::connect(): setsockopt(int, int, int, const void *, socklen_t): error code " + toStdString(errorCode) + " (" + getErrorString(errorCode) + ')');
    }

    auto connectResult = ::connect(this->m_socketDescriptor, addressInfo->ai_addr, addressInfo->ai_addrlen);
    if (connectResult == -1) {
        freeaddrinfo(addressInfo);
		auto errorCode = getLastError();
        throw std::runtime_error("CppSerialPort::TcpClient::connect(): connect(int, const sockaddr *addr, socklen_t): error code " + toStdString(errorCode) +  " (" + getErrorString(errorCode) + ')');
    }
    freeaddrinfo(addressInfo);
    this->m_readBuffer.clear();

    auto tv = toTimeVal(static_cast<uint32_t>(this->readTimeout()));
    auto readTimeoutResult = setsockopt(this->m_socketDescriptor, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&tv), sizeof(struct timeval));
    if (readTimeoutResult == -1) {
        auto errorCode = getLastError();
        throw std::runtime_error("CppSerialPort::TcpClient::connect(): setsockopt(int, int, int, const void *, int) set read timeout failed: error code " + toStdString(errorCode) + " (" + getErrorString(errorCode) + ')');
    }

    tv = toTimeVal(static_cast<uint32_t>(this->writeTimeout()));
    auto writeTimeoutResult = setsockopt(this->m_socketDescriptor, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&tv), sizeof(struct timeval));
    if (writeTimeoutResult == -1) {
        auto errorCode = getLastError();
        throw std::runtime_error("CppSerialPort::TcpClient::connect(): setsockopt(int, int, int, const void *, int) set write timeout failed: error code " + toStdString(errorCode) + " (" + getErrorString(errorCode) + ')');
    }
}

bool TcpClient::disconnect()
{
#if defined(_WIN32)
    closesocket(this->m_socketDescriptor);
#else
	close(this->m_socketDescriptor);
#endif //defined(_WIN32)
    this->m_socketDescriptor = INVALID_SOCKET;
    return true;
}

bool TcpClient::isConnected() const
{
    return this->m_socketDescriptor != INVALID_SOCKET;
}

char TcpClient::read()
{
    if (!this->m_readBuffer.empty()) {
        char returnValue{ this->m_readBuffer.front() };
        this->m_readBuffer = this->m_readBuffer.substr(1);
        return returnValue;
    }

    //Use select() to wait for data to arrive
    //At socket, then read and return
    fd_set read_fds{0, 0, 0};
    fd_set write_fds{0, 0, 0};
    fd_set except_fds{0, 0, 0};
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&except_fds);
    FD_SET(this->m_socketDescriptor, &read_fds);

    struct timeval timeout{0, 0};
    timeout.tv_sec = 0;
    timeout.tv_usec = (this->readTimeout() * 1000);
    static char readBuffer[TCP_CLIENT_BUFFER_MAX];
    memset(readBuffer, '\0', TCP_CLIENT_BUFFER_MAX);

    if (select(this->m_socketDescriptor + 1, &read_fds, &write_fds, &except_fds, &timeout) == 1) {
        auto receiveResult = recv(this->m_socketDescriptor, readBuffer, TCP_CLIENT_BUFFER_MAX - 1, 0);
        if (receiveResult == -1) {
            auto errorCode = getLastError();
            if (errorCode != EAGAIN) {
                throw std::runtime_error("CppSerialPort::TcpClient::read(): recv(int, void *, size_t, int): error code " + toStdString(errorCode) + " (" + getErrorString(errorCode) + ')');
            }
            return 0;
        } else if (strlen(readBuffer) != 0) {
            this->m_readBuffer += std::string{readBuffer};
            char returnValue{this->m_readBuffer.front()};
            this->m_readBuffer = this->m_readBuffer.substr(1);
            return returnValue;
        } else if (receiveResult == 0) {
            this->closePort();
            throw std::runtime_error("CppSerialPort::TcpClient::read(): Server [" + this->m_hostName + ':' + toStdString(this->m_portNumber) + "] hung up unexpectedly");
        }
    }
    return 0;
}

ssize_t TcpClient::write(char c)
{
    if (!this->isConnected()) {
        throw std::runtime_error("CppSerialPort::TcpClient::write(char): Cannot write on closed socket (call connect first)");
    }
    return this->write(&c, 1);
}

ssize_t TcpClient::write(const char *bytes, size_t numberOfBytes)
{
    if (!this->isConnected()) {
        throw std::runtime_error("CppSerialPort::TcpClient::write(const char *, size_t): Cannot write on closed socket (call connect first)");
    }
    unsigned sentBytes{0};
    //Make sure all bytes are sent
	auto startTime = IByteStream::getEpoch();
    while (sentBytes < numberOfBytes)  {
        auto sendResult = send(this->m_socketDescriptor, bytes + sentBytes, numberOfBytes - sentBytes, 0);
        if (sendResult == -1) {
			auto errorCode = getLastError();
            throw std::runtime_error("CppSerialPort::TcpClient::write(const char *bytes, size_t): send(int, const void *, int, int): error code " + toStdString(errorCode) + " (" + getErrorString(errorCode) + ')');
        }
        sentBytes += sendResult;
		if ( (getEpoch() - startTime) >= static_cast<unsigned int>(this->writeTimeout()) ) {
			break;
		}
    }
    return sentBytes;
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

void TcpClient::putBack(char c)
{
    this->m_readBuffer.insert(this->m_readBuffer.begin(), c);
}

timeval TcpClient::toTimeVal(uint32_t totalTimeout)
{
    timeval tv{};
    tv.tv_sec = static_cast<long>(totalTimeout / 1000);
    tv.tv_usec = static_cast<long>((totalTimeout % 1000) * 1000);
    return tv;
}

void TcpClient::setPortNumber(uint16_t portNumber) {
    if (this->isConnected()) {
        throw std::runtime_error("CppSerialPort::TcpClient::setPortNumber(uint16_t): Cannot set port number when already connected (call disconnect() first)");
    }
    this->m_portNumber = portNumber;
}

void TcpClient::setHostName(const std::string &hostName) {
    if (this->isConnected()) {
        throw std::runtime_error("CppSerialPort::TcpClient::setHostName(const std::string &): Cannot set host name when already connected (call disconnect() first)");
    }
    this->m_hostName = hostName;
}

uint16_t TcpClient::portNumber() const {
    return this->m_portNumber;
}

std::string TcpClient::hostName() const {
    return this->m_hostName;
}

} //namespace CppSerialPort
