#ifndef _TCP_ACCEPTOR_
#define _TCP_ACCEPTOR_

#include "tcp_connection.h"
#include <string>

class TCPAcceptor
{
    public:
        TCPAcceptor() : TCPAcceptor(32) {}
        explicit TCPAcceptor(int pendingConnectionsQueueSize);
        bool start(const std::string& address, int port);
        TCPConnection* accept(int timeout);
        TCPConnection* getAcceptorSocket() { return &m_socket; }

    private:
        TCPConnection m_socket;
};

#endif