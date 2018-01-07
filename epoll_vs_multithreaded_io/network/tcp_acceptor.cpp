#include "tcp_acceptor.h"
using namespace std;

TCPAcceptor::TCPAcceptor(int pendingConnectionsQueueSize)
{
    m_socket.create();
    m_socket.setPendingConnectionsQueueSize(pendingConnectionsQueueSize);
}

bool TCPAcceptor::start(const string& address, int port)
{
    m_socket.setSocketOption(SOCKET_OPTION::REUSE_ADDRESS, 1);

    if (!m_socket.bind(address, port))
    {
        return false;
    }

    if (!m_socket.listen())
    {
        return false;
    }

    return true;
}

TCPConnection* TCPAcceptor::accept(int timeout)
{
    TCPConnection* ret{ nullptr };

    ret = static_cast<TCPConnection*>(m_socket.accept(timeout));

    return ret;
}