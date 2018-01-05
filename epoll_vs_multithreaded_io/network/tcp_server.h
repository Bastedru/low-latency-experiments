#ifndef _TCP_SERVER_
#define _TCP_SERVER_

#include "tcp_connection.h"
#include <string>
#include <thread>
#include <memory>
#include <atomic>
#include <mutex>
#include <vector>
#include <cstddef>

#define DEFAULT_PENDING_CONNECTION_QUEUE_SIZE 32
#define DEFAULT_ACCEPT_TIMEOUT 5

class TCPServer
{   
    public:
        TCPServer() : TCPServer(DEFAULT_PENDING_CONNECTION_QUEUE_SIZE, DEFAULT_ACCEPT_TIMEOUT) {}

        explicit TCPServer(int pendingConnectionsQueueSize, int acceptTimeout);
        virtual ~TCPServer() { stop(); }

        virtual bool start(const std::string& address, int port);
        virtual void stop();
        virtual void* acceptorThread();
        virtual void onClientDisconnected(std::size_t peerIndex);
        virtual void onClientConnected(std::size_t peerIndex);
        virtual void onUnhandledSocketError(int errorCode);
        virtual void clientHandlerThread(std::size_t peerIndex); // For thread-per-client servers
        virtual void onClientReady(std::size_t peerIndex);       // For reactor servers
        
        TCPConnection* getAcceptorSocket() { return &m_socket; }
        TCPConnection* getPeerSocket(std::size_t peerIndex) { return m_peerSockets[peerIndex].get(); }
        
    protected:

        std::unique_ptr<std::thread> m_acceptorThreadPtr;
        int m_acceptTimeout;
        std::atomic<bool> m_isStopping;
        std::vector<std::unique_ptr<TCPConnection>> m_peerSockets;
        std::vector<bool> m_peerSocketsConnectionFlags;
        mutable std::mutex m_peerSocketsLock;
        TCPConnection m_socket;

        std::size_t addPeer(TCPConnection* peer);
};

#endif
