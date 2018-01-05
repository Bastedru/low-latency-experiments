#include "network/socket_library.h"
#include "network/tcp_connector.h"
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <sstream>
#include <mutex>
#include <chrono>
#if __linux__
#include <termios.h>
#endif`
using namespace std;
using namespace std::chrono;

void clientThread();
int messageCounter = 0;
mutex messageCounterMutex;
int messageIteration = 0;
int maxConnectionTrials = 1000;

int main()
{
    SocketLibrary::initialise();

    cout << "Enter client number :";
    string userInput;
    cin >> userInput;
    auto numberOfClients = std::stoi(userInput);

    cout << "Enter message iteration number :";
    cin >> userInput;
    messageIteration = std::stoi(userInput);

    vector<thread*> threads;
    std::chrono::high_resolution_clock::time_point startTime = high_resolution_clock::now();

    for (auto i = 0; i < numberOfClients; i++)
    {
        threads.emplace_back(new thread(clientThread));
    }

    for (auto& thread : threads)
    {
        thread->join();
    }
    
    std::chrono::high_resolution_clock::time_point endTime = high_resolution_clock::now();
    
    long long elapsedMilliseconds = duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    cout << "Elapsed time is " << elapsedMilliseconds << "milliseconds" << endl;

#ifdef _WIN32
    system("pause");
#elif __linux__
    std::cout << std::endl << "Press any key to continue..." << std::endl;
    struct termios info;
    tcgetattr(0, &info);          /* get current terminal attirbutes; 0 is the file descriptor for stdin */
    info.c_lflag &= ~ICANON;      /* disable canonical mode */
    info.c_cc[VMIN] = 1;          /* wait until at least one keystroke available */
    info.c_cc[VTIME] = 0;         /* no timeout */
    tcsetattr(0, TCSANOW, &info); /* set immediately */
#endif
    
    SocketLibrary::uninitialise();
    return 0;
}

void clientThread()
{
    TCPConnector connector;

    TCPConnection* serverSocket{ nullptr };
    
    int numberTrials{ 0 };

    while (true)
    {
        serverSocket = connector.connect("127.0.0.1", 666);
        if (serverSocket)
        {
            break;
        }
        else
        {
            numberTrials++;
            if (numberTrials == maxConnectionTrials)
            {
                cout << endl << "Max connection trial reached" << endl;
                break;
            }
        }
    }
    
    if (serverSocket)
    {
        for (int i = 0; i < messageIteration; i++)
        {
            stringstream message;
            messageCounterMutex.lock();
            messageCounter++;
            message << "message" << messageCounter;
            serverSocket->send(message.str());
            messageCounterMutex.unlock();

            char buffer[33];
            std::memset(buffer, 0, 33);
            auto res = serverSocket->receive(buffer, 32, 5);

            if (res >= 0)
            {
                if (res > 0)
                {
                    buffer[res] = '\0';
                    //cout << endl << "Received from server : " << buffer << endl;
                }
            }
            else
            {
                cout << endl << "Server disconnected" << endl;
                break;
            }
        }

        serverSocket->send("quit");
    }
    else
    {
        cout << endl << "Could not connect to server" << endl << endl;
    }
}
