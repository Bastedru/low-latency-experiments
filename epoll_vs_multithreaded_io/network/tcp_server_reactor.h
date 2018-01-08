#ifndef _TCP_SERVER_REACTOR_
#define _TCP_SERVER_REACTOR_

#include "socket.h"
#include "tcp_server.h"

#ifdef __linux__
#include "io_listener_epoll.h"
#include <unordered_map>
#elif _WIN32
#include "io_listener_select.h"
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
        void setMaxPollEvents(std::size_t maxPollEvents);
        virtual bool start(const std::string& address, int port) override;
        virtual void stop() override;
        virtual void onClientDisconnected(std::size_t peerIndex) override;
        virtual void onClientConnected(std::size_t peerIndex) override;
        virtual void* reactorThread();
    protected :
        std::unique_ptr<std::thread> m_reactorThread;

#ifdef __linux__
        IOListenerEpoll m_ioListener;
        std::unordered_map<int, std::size_t> m_peerSocketIndexTable;
#elif _WIN32
        IOListenerSelect m_ioListener;
#endif

};

#endif