#include "tcp_server_reactor.h"
#include <string>
#include <cstring>
using namespace std;

TCPServerReactor::TCPServerReactor(int pendingConnectionsQueueSize, int acceptTimeout)
: TCPServer(pendingConnectionsQueueSize, acceptTimeout)
{
}

void TCPServerReactor::stop()
{
    m_isStopping.store(true);

    if (m_reactorThread.get())
    {
        if (m_reactorThread->native_handle())
        {
            m_reactorThread->join();
        }
    }

#ifdef __linux__
    m_ioListener.stop();
#endif

}

void TCPServerReactor::setPollTimeout(long microSeconds)
{
    m_ioListener.setTimeout(microSeconds);
}

bool TCPServerReactor::start(const string& address, int port)
{
    // Start acceptor thread
    if (TCPServer::start(address, port) == false)
    {
        return false;
    }

#ifdef __linux__
    m_ioListener.start();
#endif

    // Start reactor thread
    m_reactorThread.reset(new std::thread(&TCPServerReactor::reactorThread, this));
    return true;
}

void TCPServerReactor::onClientConnected(size_t peerIndex)
{
#ifdef __linux__
    auto sd = m_peerSockets[peerIndex]->getSocketDescriptor();
    m_peerSocketIndexTable[sd] = peerIndex;
    m_ioListener.addFileDescriptor(sd);
#endif
}

void TCPServerReactor::onClientDisconnected(size_t peerIndex)
{
    auto peerSocket = m_peerSockets[peerIndex].get();

#ifdef __linux__
    m_ioListener.removeFileDescriptor(peerSocket->getSocketDescriptor());
#elif _WIN32
    m_ioListener.clearFileDescriptor(peerSocket->getSocketDescriptor());
#endif

    m_peerSocketsConnectionFlags[peerIndex] = false;
    peerSocket->close();
}

void* TCPServerReactor::reactorThread()
{
    while (true)
    {
        std::lock_guard<std::mutex> guard(m_peerSocketsLock);

        if (m_isStopping.load() == true)
        {
            break;
        }

        if (m_socket.getState() != SOCKET_STATE::ACCEPTED)
        {
            continue;
        }

        size_t connectedPeerCount{ 0 };
        int maxSocketDescriptor{-1};

        auto peerCount = m_peerSockets.size();
        for (size_t i{ 0 }; i < peerCount; i++)
        {
           int currentSocketDescriptor = m_peerSockets[i]->getSocketDescriptor();
           if (m_peerSocketsConnectionFlags[i] == true)
           {
                if(currentSocketDescriptor > maxSocketDescriptor)
                {
                    maxSocketDescriptor = currentSocketDescriptor;
                }
#ifdef _WIN32
                m_ioListener.setFileDescriptor(currentSocketDescriptor);
#endif
                connectedPeerCount++;
            }
#ifdef _WIN32
            else
            {
                m_ioListener.clearFileDescriptor(currentSocketDescriptor);
            }
#endif
        }

        if (connectedPeerCount == 0)
        {
            continue;
        }
        int result{ -1 };

#ifdef __linux__
        result = m_ioListener.getNumberOfReadyFileDescriptors();
#elif _WIN32
        result = m_ioListener.eventReady(maxSocketDescriptor);
#endif

        if (result > 0)
        {
#ifdef __linux__
            // We only iterate thru events with epoll instead of all socket descriptors
            auto numEvents = result;
            for (int counter{ 0 }; counter < numEvents; counter++)
            {
                size_t peerIndex = m_peerSocketIndexTable[m_ioListener.getReadyFileDescriptor(counter)];
                onClientReady(peerIndex);
            }
#elif _WIN32
            // We have to iterate thru all sockets with select
            for (int counter{ 0 }; counter < peerCount; counter++)
            {
                if (m_ioListener.isFileDescriptorReady(m_peerSockets[counter]->getSocketDescriptor()))
                {
                    onClientReady(counter);
                }
            }
#endif
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