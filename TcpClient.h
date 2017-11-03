//
// Created by pinguinsan on 11/3/17.
//

#ifndef CPPTCP_TCPCLIENT_H
#define CPPTCP_TCPCLIENT_H

#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <memory>
#include "IByteStream.h"

class TcpClient : public CppSerialPort::IByteStream
{
public:
    TcpClient(const std::string &hostName, uint16_t portNumber);

    ~TcpClient() override;

    int read() override;
    ssize_t write(int i) override;
    ssize_t writeLine(const std::string &str) override;
    std::string portName() const override;
    bool isOpen() const override;
    void openPort() override;
    void closePort() override;
    void flushRx() override;
    void flushTx() override;
    void putBack(int c) override;

    void connect(const std::string &hostName, uint16_t portNumber);
    void connect();
    bool disconnect();
    bool isConnected() const;
    void setPortNumber(uint16_t portNumber);
    void setHostName(const std::string &hostName);
    uint16_t portNumber() const;
    std::string hostName() const;
private:
    int m_socketDescriptor;
    uint16_t m_portNumber;
    std::string m_hostName;
    std::string m_readBuffer;
    bool m_readTimeoutSet;

    static timeval toTimeVal(uint32_t totalTimeout);

};


#endif //CPPTCP_TCPCLIENT_H
