#ifndef _TCP_SERVER_REACTOR_
#define _TCP_SERVER_REACTOR_

#include "socket.h"
#include "tcp_server.h"

#ifdef __linux__
#include <sys/time.h>
#define USE_EPOLL
#ifdef USE_EPOLL
#include <sys/epoll.h>
#include <unordered_map>
#endif
#endif

#include <string>
#include <thread>
#include <memory>
#include <cstddef>

class TCPServerReactor : public TCPServer
{   
    public:
        TCPServerReactor(): TCPServerReactor(DEFAULT_PENDING_CONNECTION_QUEUE_SIZE, DEFAULT_ACCEPT_TIMEOUT){}
        explicit TCPServerReactor(int pendingConnectionsQueueSize, int acceptTimeout);
        void setPollTimeout(long microSeconds);
        virtual bool start(const std::string& address, int port) override;
        virtual void stop() override;
        virtual void onClientDisconnected(std::size_t peerIndex) override;
		virtual void onClientConnected(std::size_t peerIndex) override;
        virtual void* reactorThread();
    protected :
        std::unique_ptr<std::thread> m_reactorThread;
		
		#ifdef USE_EPOLL
		int m_epollDescriptor;
		struct epoll_event* m_epollEvents;
		int m_epollTimeout;
		#define MAX_POLL_EVENTS 64
		std::unordered_map<int, std::size_t> m_peerSocketIndexTable;
		#else 
        struct timeval m_timeout;
		fd_set m_clientsReadSet;
		#endif
};

#endif
