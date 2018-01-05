#include "tcp_server_reactor.h"
#include <string>
#include <cstring>
using namespace std;

TCPServerReactor::TCPServerReactor(int pendingConnectionsQueueSize, int acceptTimeout)
: TCPServer(pendingConnectionsQueueSize, acceptTimeout)
{
	#ifdef USE_EPOLL
	m_epollDescriptor = -1;
	m_epollEvents = nullptr;
	#else 
	FD_ZERO(&m_clientsReadSet);
	#endif
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
	
	#ifdef USE_EPOLL
	if(m_epollEvents)
	{
		delete[] m_epollEvents;
	}
	#endif
}

void TCPServerReactor::setPollTimeout(long microSeconds)
{
	#ifdef USE_EPOLL
	m_epollTimeout = microSeconds / 1000;
	#else 
	m_timeout.tv_sec = (microSeconds / 1000000);
    m_timeout.tv_usec = microSeconds % 1000000;
	#endif
}

bool TCPServerReactor::start(const string& address, int port)
{
    // Start acceptor thread
    if (TCPServer::start(address, port) == false)
    {
        return false;
    }
	
	#ifdef USE_EPOLL
	m_epollDescriptor = epoll_create1(0);
	
	m_epollEvents = new struct epoll_event[MAX_POLL_EVENTS];
	
	#else
    // DEFAULT TIMEOUT IF NOT SET
    if (m_timeout.tv_sec == 0 && m_timeout.tv_usec == 0)
    {
        setPollTimeout(50000);
    }
	#endif
    
    // Start reactor thread
    m_reactorThread.reset(new std::thread(&TCPServerReactor::reactorThread, this));
    return true;
}

void TCPServerReactor::onClientConnected(size_t peerIndex)
{
#ifdef USE_EPOLL
	auto sd = m_peerSockets[peerIndex]->getSocketDescriptor();
	
	m_peerSocketIndexTable[sd] = peerIndex;
	
	struct epoll_event epollSocketDescriptor;
	epollSocketDescriptor.data.fd = sd;
	epollSocketDescriptor.events = EPOLLIN | EPOLLET; // Edge trigger mode
	
	epoll_ctl(m_epollDescriptor, EPOLL_CTL_ADD, sd, &epollSocketDescriptor);
#endif
}

void TCPServerReactor::onClientDisconnected(size_t peerIndex)
{
	auto peerSocket = m_peerSockets[peerIndex].get();
	#ifdef USE_EPOLL
	struct epoll_event epollSocketDescriptor;
	auto sd = peerSocket->getSocketDescriptor();
	epollSocketDescriptor.data.fd = sd;
	epollSocketDescriptor.events = EPOLLIN;
	epoll_ctl(m_epollDescriptor, EPOLL_CTL_DEL, peerSocket->getSocketDescriptor(), &epollSocketDescriptor);
	#else
    FD_CLR(peerSocket->getSocketDescriptor(), &m_clientsReadSet);
	#endif
    m_peerSocketsConnectionFlags[peerIndex] = false;
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
           if (m_peerSocketsConnectionFlags[i] == true)
           {
                int currentSocketDescriptor = m_peerSockets[i]->getSocketDescriptor();
                if(currentSocketDescriptor > maxSocketDescriptor)
                {
                    maxSocketDescriptor = currentSocketDescriptor;
                }
				#ifndef USE_EPOLL
                FD_SET(currentSocketDescriptor, &m_clientsReadSet);
				#endif
                connectedPeerCount++;
            }   
			#ifndef USE_EPOLL
            else
            {
                FD_CLR(m_peerSockets[i]->getSocketDescriptor(), &m_clientsReadSet);
            }   
			#endif
        }
        
        if (connectedPeerCount == 0)
        {
            continue;
        }
        int result{ -1 };

		#ifdef USE_EPOLL
		result = ::epoll_wait(m_epollDescriptor, m_epollEvents, MAX_POLL_EVENTS, m_epollTimeout);
		#else
        result = ::select(maxSocketDescriptor+1, &m_clientsReadSet, nullptr, nullptr, &m_timeout);
		#endif

        if (result > 0)
        {
			#ifdef USE_EPOLL
			// We only iterate thru events with epoll instead of all socket descriptors
			auto numEvents = result;
			for (int counter{ 0 }; counter < numEvents; counter++)
            {
				size_t peerIndex = m_peerSocketIndexTable[ m_epollEvents[counter].data.fd ];  
				onClientReady(peerIndex);
			}
			#else
            // We have to iterate thru all sockets with select
            for (int counter{ 0 }; counter < peerCount; counter++)
            {
                if (FD_ISSET(m_peerSockets[counter]->getSocketDescriptor(), &m_clientsReadSet))
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