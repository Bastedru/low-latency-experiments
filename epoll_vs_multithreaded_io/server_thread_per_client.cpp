#include "network/socket_library.h"
#include "network/socket.h"
#include "network/tcp_server_thread_per_client.h"

#include <cstddef>
#include <cstring>
#include <iostream>
#include <sstream>
#include <mutex>
using namespace std;

class TcpServerMultithreadedTest : public TCPServerThreadPerClient
{
private :
    int m_clientCounter;
    mutex m_clientCounterMutex;
public:

    TcpServerMultithreadedTest() : m_clientCounter{0}
    {
    }

    virtual void onClientConnected(size_t peerIndex) override
    {
        m_clientCounterMutex.lock();
        m_clientCounter++;
        m_clientCounterMutex.unlock();
        //cout << "Client" << peerIndex << " connected : " << m_clientCounter << "TH CONNECTION" << endl;
        TCPServerThreadPerClient::onClientConnected(peerIndex);
    }

    virtual void onUnhandledSocketError(int errorCode) override
    {
        cout << "Unhandled socket error occured : " << errorCode << endl;
    }

    virtual void clientHandlerThread(size_t peerIndex) override
    {
        while (true)
        {
            if (m_isStopping.load() == true)
            {
                break;
            }
            
            auto peerSocket = getPeerSocket(peerIndex);

            char buffer[33];
            std::memset(buffer, 0, 33);
            int read{ -1 };
            {
                std::lock_guard<std::mutex> guard(m_peerSocketsLock);

#if _WIN32
                read = peerSocket->receive(buffer,32, 10);
#elif __linux__
                read = peerSocket->receive(buffer, 32);
#endif
            }

            auto error = Socket::getCurrentThreadLastSocketError();

            if (error == 0 && read > 0)
            {
                buffer[read] = '\0';
                //cout << endl << "Received from client " << peerIndex << " : " << buffer << endl;

                stringstream execReport;
                execReport << "Exec report ";
                peerSocket->send(execReport.str());

                if (!strcmp(buffer, "quit"))
                {
                    onClientDisconnected(peerIndex);
                    //cout << "Client" << peerIndex << " disconnected" << endl;
                }
            }
            else
            {
                if (peerSocket->isConnectionLost(error, read))
                {
                    onClientDisconnected(peerIndex);
                    //cout << "Client" << peerIndex << " disconnected" << endl;
                    break;
                }
                else if( error != 0)
                {
                    onUnhandledSocketError(error);
                    continue;
                }
            }
        }
    }
};

int main()
{
    SocketLibrary::initialise();
    cout << "Starting" << endl;
    TcpServerMultithreadedTest server;
    server.start("127.0.0.1", 666);

    while (true)
    {
        std::string value;
        std::cin >> value;

        if (value == "quit")
        {
            cout << endl << "Quitting..." << endl << endl;
            break;
        }
    }

    server.stop();
    SocketLibrary::uninitialise();
    return 0;
}
