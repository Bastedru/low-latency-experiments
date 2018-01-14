#include "tcp_connection.h"
#ifdef TCP_DEBUG
#include <fstream>
#include <sstream>
#include <iostream>
#endif
using namespace std;

bool TCPConnection::create()
{
    return Socket::create(SOCKET_TYPE::TCP);
}

int TCPConnection::send(const string& buffer, int timeout)
{
    return send(buffer.c_str(), buffer.length(), timeout);
}

int TCPConnection::send(const char* buffer, size_t len, int timeout)
{
    if (timeout > 0)
    {
        if (select(false, true, timeout) == false)
        {
            return 0;
        }
    }

    auto result = ::send(m_socketDescriptor, buffer, len, 0);

#ifdef TCP_DEBUG
    m_sendCounter++;
    stringstream log;
    log << "send " << m_sendCounter << " : " << buffer << endl;
    socketDebugLog(log.str());
#endif

    return result;
}

int TCPConnection::receive(char* buffer, size_t len, int timeout)
{
    if (timeout > 0)
    {
        if (select(true, false, timeout) == false)
        {
            return 0;
        }
    }

    auto result = ::recv(m_socketDescriptor, buffer, len, 0);

#ifdef TCP_DEBUG
    m_receiveCounter++;
    stringstream log;
    log << "recv " << m_receiveCounter << " : " << buffer << endl;
    socketDebugLog(log.str());
#endif

    return result;
}

void TCPConnection::connectionDebugLog(const string& logMessage)
{
#ifdef TCP_DEBUG
    stringstream fileName;
    fileName << getName() << ".txt";
    ofstream file;
    file.open(fileName.str(), std::ios_base::app);
    file << logMessage << std::endl;
    file.close();
#endif
}