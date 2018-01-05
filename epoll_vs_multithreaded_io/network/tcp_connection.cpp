#include "tcp_connection.h"
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
    if (timeout <= 0) return ::send(m_socketDescriptor, buffer, len, 0);

    if (select(false, true, timeout) == true)
    {
        return ::send(m_socketDescriptor, buffer, len, 0);
    }
    return 0;
}

int TCPConnection::receive(char* buffer, size_t len, int timeout)
{
    if (timeout <= 0) return recv(m_socketDescriptor, buffer, len, 0);

    if (select(true, false, timeout) == true)
    {
        return recv(m_socketDescriptor, buffer, len, 0);
    }

    return 0;
}