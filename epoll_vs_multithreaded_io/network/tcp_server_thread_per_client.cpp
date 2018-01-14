#include "tcp_server_thread_per_client.h"
using namespace std;

bool TCPServerThreadPerClient::start(const string& address, int port)
{
    if (TCPServer::start(address, port) == false)
    {
        return false;
    }

    m_acceptorThreadPtr.reset(new std::thread(&TCPServerThreadPerClient::acceptThread, this));

    return true;
}

void TCPServerThreadPerClient::stop()
{
    TCPServer::stop();

    if (m_acceptorThreadPtr.get())
    {
        if (m_acceptorThreadPtr->native_handle())
        {
            m_acceptorThreadPtr->join();
        }
    }

    for (auto& clientThread : m_clientThreads)
    {
        if (clientThread.get())
        {
            if (clientThread->native_handle())
            {
                clientThread->join();
            }
        }
    }
}

void TCPServerThreadPerClient::onClientConnected(size_t peerIndex)
{
    m_clientThreads.emplace_back(new thread(&TCPServerThreadPerClient::handleClient, this, peerIndex));
}

void TCPServerThreadPerClient::onClientDisconnected(size_t peerIndex)
{
    std::lock_guard<std::mutex> guard(m_peerSocketsLock);
    removePeer(peerIndex);
}

void* TCPServerThreadPerClient::acceptThread()
{
    int peerCounter{ 0 };
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