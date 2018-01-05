#include "tcp_connector.h"
using namespace std;

TCPConnection* TCPConnector::connect(const string& server, int port)
{
    m_socket.create();

    if (!m_socket.connect(server, port))
    {
        return nullptr;
    }

    return &m_socket;
}

TCPConnection* TCPConnector::connect(const string& server, int port, int timeout)
{
    m_socket.create();
    if (!m_socket.connect(server, port, timeout))
    {
        return nullptr;
    }

    return &m_socket;
}