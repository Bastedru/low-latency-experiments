#include "tcp_server.h"
using namespace std;

TCPServer::TCPServer(int pendingConnectionsQueueSize, int acceptTimeout) 
: m_acceptorThreadPtr{ nullptr }, m_acceptTimeout{ acceptTimeout }
{
    m_isStopping.store(false);
    m_socket.create();
    m_socket.setPendingConnectionsQueueSize(pendingConnectionsQueueSize);
}

bool TCPServer::start(const string& address, int port)
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

    m_acceptorThreadPtr.reset(new std::thread(&TCPServer::acceptorThread, this));

    return true;
}

void TCPServer::stop()
{
    m_isStopping.store(true);
    
    if (m_acceptorThreadPtr.get())
    {
        if (m_acceptorThreadPtr->native_handle())
        {
            m_acceptorThreadPtr->join();
        }
    }
}

void* TCPServer::acceptorThread()
{
    while (true)
    {
        
        if (m_isStopping.load() == true)
        {
            break;
        }

        auto peerSocket = static_cast<TCPConnection*>(m_socket.accept(m_acceptTimeout));

        if (peerSocket)
        {
            std::lock_guard<std::mutex> guard(m_peerSocketsLock);
            auto peerIndex = addPeer(peerSocket);
            onClientConnected(peerIndex);
        }
    }
    return nullptr;
}

void TCPServer::onClientDisconnected(size_t peerIndex)
{
    std::lock_guard<std::mutex> guard(m_peerSocketsLock);
    m_peerSocketsConnectionFlags[peerIndex] = false;
}

void TCPServer::onClientConnected(std::size_t peerIndex)
{
    // MEANT TO BE OVERRIDDEN
}


void TCPServer::onUnhandledSocketError(int errorCode)
{
    // MEANT TO BE OVERRIDDEN
}

void TCPServer::clientHandlerThread(std::size_t peerIndex)
{
    // MEANT TO BE OVERRIDDEN FOR THREAD-PER-CLIENT TYPE SERVERS
    // ALSO OVERRIDERS HAVE TO CALL onUnhandledSocketError and onClientDisconnected
}

void TCPServer::onClientReady(std::size_t peerIndex)
{
    // MEANT TO BE OVERRIDDEN FOR REACTOR(POLL/SELECT/EPOLL) TYPE SERVERS
    // ALSO OVERRIDERS HAVE TO CALL onUnhandledSocketError and onClientDisconnected
}

size_t TCPServer::addPeer(TCPConnection* peer)
{
    auto currentSize = m_peerSockets.size();
    int nonUserPeerIndex{ -1 };
    int ret{ -1 };

    for (size_t i{ 0 }; i < currentSize; i++)
    {
        if (m_peerSocketsConnectionFlags[i] == false)
        {
            nonUserPeerIndex = i;
            break;
        }
    }

    if (nonUserPeerIndex == -1) 
    {
        // No empty slot , create new
        m_peerSocketsConnectionFlags.push_back(true);
        m_peerSockets.emplace_back(peer);
        ret = m_peerSockets.size() - 1;
    }
    else
    {
        // Use an existing peer slot
        m_peerSockets[nonUserPeerIndex]->close();
        m_peerSockets[nonUserPeerIndex].reset(peer);
        m_peerSocketsConnectionFlags[nonUserPeerIndex] = true;
        ret = nonUserPeerIndex;
    }
    return ret;
}