#include "tcp_connection.h"
#ifdef SOCKET_DEBUG
#include <sstream>
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

#ifdef SOCKET_DEBUG
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

#ifdef SOCKET_DEBUG
    m_receiveCounter++;
    stringstream log;
    log << "recv " << m_receiveCounter << " : " << buffer << endl;
    socketDebugLog(log.str());
#endif

    return result;
}