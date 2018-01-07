#include "tcp_server_thread_per_client.h"

using namespace std;

void TCPServerThreadPerClient::stop()
{
    m_isStopping.store(true);
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
    m_clientThreads.emplace_back(new thread(&TCPServerThreadPerClient::clientHandlerThread, this, peerIndex));
}