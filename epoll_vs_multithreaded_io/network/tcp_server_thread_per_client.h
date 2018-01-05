#ifndef _TCP_SERVER_THREAD_PER_CLIENT_
#define _TCP_SERVER_THREAD_PER_CLIENT_

#include "socket.h"
#include "tcp_server.h"
#include <thread>
#include <memory>
#include <vector>
#include <cstddef>

class TCPServerThreadPerClient : public TCPServer
{   
    public:
        TCPServerThreadPerClient() : TCPServerThreadPerClient(DEFAULT_PENDING_CONNECTION_QUEUE_SIZE, DEFAULT_ACCEPT_TIMEOUT) {}
        explicit TCPServerThreadPerClient(int pendingConnectionsQueueSize, int acceptTimeout){}
        virtual void stop() override;
        virtual void onClientConnected(std::size_t peerIndex) override;
        virtual void onClientDisconnected(std::size_t peerIndex) override;
    protected :
        std::vector<std::unique_ptr<std::thread>> m_clientThreads;
};

#endif