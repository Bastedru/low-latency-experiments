#include "tcp_server_reactor.h"
#include <string>
#include <cstring>
#include <pthread.h>
using namespace std;

TCPServerReactor::TCPServerReactor(int pendingConnectionsQueueSize, int acceptTimeout)
: TCPServer(pendingConnectionsQueueSize, acceptTimeout)
{
}

bool TCPServerReactor::start(const string& address, int port)
{
    // Start acceptor thread
    if (TCPServer::start(address, port) == false)
    {
        return false;
    }

    m_socket.setBlockingMode(false);

    m_ioListener.start();
    m_ioListener.addFileDescriptor(m_socket.getSocketDescriptor());

    // Start reactor thread
    m_reactorThread.reset(new std::thread(&TCPServerReactor::reactorThread, this));
    return true;
}

void TCPServerReactor::stop()
{
    TCPServer::stop();

    if (m_reactorThread.get())
    {
        if (m_reactorThread->native_handle())
        {
            m_reactorThread->join();
        }
    }

    m_ioListener.stop();
}

void TCPServerReactor::setPollTimeout(long microSeconds)
{
    m_ioListener.setTimeout(microSeconds);
}

void TCPServerReactor::setMaxPollEvents(size_t maxPollEvents)
{
    m_ioListener.setMaxPollEvents(maxPollEvents);
}

void TCPServerReactor::onClientConnected(size_t peerIndex)
{
    auto sd = m_peerSockets[peerIndex]->getSocketDescriptor();
    m_peerSocketIndexTable[sd] = peerIndex;
    m_ioListener.addFileDescriptor(sd);
    m_peerSockets[peerIndex]->setBlockingMode(false);
}

void TCPServerReactor::onClientDisconnected(size_t peerIndex)
{
    auto peerSocket = m_peerSockets[peerIndex].get();
    m_ioListener.removeFileDescriptor(peerSocket->getSocketDescriptor());
    removePeer(peerIndex);
}

size_t TCPServerReactor::acceptNewConnection()
{
    size_t peerIndex{ 0 };
    TCPConnection* peerSocket = nullptr;

    peerSocket = static_cast<TCPConnection*>(m_socket.accept(m_acceptTimeout));

    if (peerSocket)
    {
        auto peerIndex = addPeer(peerSocket);
        onClientConnected(peerIndex);
    }

    return peerIndex;
}

void* TCPServerReactor::reactorThread()
{
    // Set thread name so looks nicely in stap output
    pthread_setname_np(m_reactorThread->native_handle(), "reactor_thread");
    while (true)
    {
        if (m_isStopping.load() == true)
        {
            break;
        }

        int result{ -1 };
        TCPConnection* newPeerSocket = nullptr;

        // Client handle events
        result = m_ioListener.getNumberOfReadyFileDescriptors();

        if (result > 0)
        {
            // We only iterate thru events with epoll instead of all socket descriptors
            auto numEvents = result;
            for (int counter{ 0 }; counter < numEvents; counter++)
            {
                size_t peerIndex = m_peerSocketIndexTable[m_ioListener.getReadyFileDescriptor(counter)];

                if (m_ioListener.isValidEvent(counter))
                {
                    auto currentSocketDescriptor = m_ioListener.getReadyFileDescriptor(counter);
                    if (currentSocketDescriptor == m_socket.getSocketDescriptor())
                    {
                        acceptNewConnection();
                    }
                    else
                    {
                        handleClient(peerIndex);
                    }
                }
                else
                {
                    onClientDisconnected(peerIndex);
                }
            }
        }
        else if (result == 0)
        {
            // Timeout
        }
        else
        {
            auto errorCode = Socket::getCurrentThreadLastSocketError();
            onUnhandledSocketError(errorCode);
        }
    }
    return nullptr;
}