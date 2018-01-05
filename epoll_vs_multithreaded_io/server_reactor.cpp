#include "network/socket_library.h"
#include "network/socket.h"
#include "network/tcp_server_reactor.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <mutex>
using namespace std;

class TCPServerReactorTest : public TCPServerReactor
{
    private :
        int m_clientCounter;
        mutex m_clientCounterMutex;
    public :

        TCPServerReactorTest() : m_clientCounter{0}
        {
        }

        virtual void onUnhandledSocketError(int errorCode) override
        {
            cout << "Unhandled socket error occured : " << errorCode << endl;
        }

        virtual void onClientConnected(size_t peerIndex) override
        {
            m_clientCounterMutex.lock();
            m_clientCounter++;
            m_clientCounterMutex.unlock();
            
            //cout << "Client" << peerIndex << " connected : " << m_clientCounter << "TH CONNECTION" << endl;
            
            TCPServerReactor::onClientConnected(peerIndex);
        }

        virtual void onClientReady(size_t peerIndex) override
        {
            //cout << "Client" << peerIndex << " event ready" << endl;

            auto peerSocket = getPeerSocket(peerIndex);
            char buffer[33];
            std::memset(buffer, 0, 33);
#if _WIN32
            auto read = peerSocket->receive(buffer, 32, 10);
#elif __linux__
            auto read = peerSocket->receive(buffer, 32);
#endif
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
                    //cout << "Client" << peerIndex << " disconnected" << endl;
                    onClientDisconnected(peerIndex);
                }
                else if (error != 0)
                {
                    onUnhandledSocketError(error);
                }
            }
        }
};

int main()
{
    SocketLibrary::initialise();

    TCPServerReactorTest server;
    server.setPollTimeout(50000);
    server.start("127.0.0.1", 666);

    while (true)
    {
        std::string value;
        std::cin >> value;

        if (value == "quit")
        {
            break;
        }
    }
    server.stop();
    SocketLibrary::uninitialise();
    return 0;
}
