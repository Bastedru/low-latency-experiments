#ifndef _TCP_CONNECTOR_H
#define _TCP_CONNECTOR_H

#include <string>
#include "tcp_connection.h"

class TCPConnector
{
    public:
        TCPConnection* connect(const std::string& server, int port);
        TCPConnection* connect(const std::string& server, int port, int timeout);
    private:
        TCPConnection m_socket;
};

#endif