#ifndef CPPSERIALPORT_TCPCLIENT_H
#define CPPSERIALPORT_TCPCLIENT_H

#include <string>
#if defined(_WIN32)
#    include "WinSock2.h"
#    include "Windows.h"
#else
#    include <sys/socket.h>
#    include <netinet/in.h>
#    include <netdb.h>
#endif //defined(_WIN32)


#include <sys/types.h>
#include <memory>
#include "IByteStream.h"

namespace CppSerialPort {

class TcpClient : public IByteStream
{
public:
    TcpClient(const std::string &hostName, uint16_t portNumber);
    ~TcpClient() override;

    char read() override;
    ssize_t write(char i) override;
	ssize_t write(const char *bytes, size_t numberOfBytes) override;
    std::string portName() const override;
    bool isOpen() const override;
    void openPort() override;
    void closePort() override;
    void flushRx() override;
    void flushTx() override;
    void putBack(char c) override;

    void connect(const std::string &hostName, uint16_t portNumber);
    void connect();
    bool disconnect();
    bool isConnected() const;
    void setPortNumber(uint16_t portNumber);
    void setHostName(const std::string &hostName);
    uint16_t portNumber() const;
    std::string hostName() const;
private:
#if defined(_WIN32)
	SOCKET m_socketDescriptor;
#else
	int m_socketDescriptor;
#endif //defined(_WIN32)
    uint16_t m_portNumber;
    std::string m_hostName;
    std::string m_readBuffer;

    static timeval toTimeVal(uint32_t totalTimeout);
	static std::string getErrorString(int errorCode);
	static int getLastError();

};

} //namespace CppSerialPort

#endif //CPPTCP_TCPCLIENT_H
