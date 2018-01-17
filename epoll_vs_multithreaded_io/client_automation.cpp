#include "network/socket_library.h"
#include "network/tcp_connector.h"
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <sstream>
#include <atomic>
#include <mutex>
#include <chrono>
#include <memory>
#include <termios.h>
using namespace std;
using namespace std::chrono;

#define BENCHMARK

#define IP_ADDRESS "127.0.0.1"
#define PORT 666

void clientThread();
int messageCounter = 0;
mutex messageCounterMutex;
int numberOfClients = 0;
int messageIteration = 0;
int maxConnectionTrials = 10;
atomic<int> numberDisconnections;

int main()
{
    SocketLibrary::initialise();
    numberDisconnections.store(0);

    cout << "Enter client number :";
    string userInput;
    cin >> userInput;
    numberOfClients = std::stoi(userInput);

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
    long long elapsedMicroseconds = elapsedMilliseconds * 1000;

    double averageRTTtime = (double)elapsedMicroseconds / (numberOfClients * messageIteration);

    cout << "Elapsed time is " << elapsedMilliseconds << " milliseconds" << endl << endl;
    cout << "Average RTT time is " << averageRTTtime << " microseconds" << endl << endl;
    cout << "Number of disconnections is " << numberDisconnections << endl << endl;

    std::cout << std::endl << "Press any key to continue..." << std::endl;
    struct termios info;
    tcgetattr(0, &info);          /* get current terminal attirbutes; 0 is the file descriptor for stdin */
    info.c_lflag &= ~ICANON;      /* disable canonical mode */
    info.c_cc[VMIN] = 1;          /* wait until at least one keystroke available */
    info.c_cc[VTIME] = 0;         /* no timeout */
    tcsetattr(0, TCSANOW, &info); /* set immediately */

    SocketLibrary::uninitialise();
    return 0;
}

void clientThread()
{
    unique_ptr<TCPConnector> connector(new TCPConnector);

    TCPConnection* serverSocket{ nullptr };

    int numberTrials{ 0 };

    while (true)
    {
        serverSocket = connector->connect(IP_ADDRESS, PORT);
        if (serverSocket)
        {
            break;
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
            int res = -1;

            while (res != 0)
            {
                res = serverSocket->receive(buffer, 32);
                auto error = Socket::getCurrentThreadLastSocketError();
                if (res > 0)
                {
                    buffer[res] = '\0';
#ifndef BENCHMARK
                    cout << endl << "Received from server : " << buffer << endl;
#endif
                    break;
                }
                else
                {
                    if(serverSocket->isConnectionLost(error, res))
                    {
                        serverSocket->close();
                        connector.reset(new TCPConnector);
                        ++numberDisconnections;
                        while (true)
                        {
                            serverSocket = connector->connect(IP_ADDRESS, PORT);
                            if (serverSocket)
                            {
                                serverSocket->send(message.str());
                                break;
                            }
                        }
                    }
                }
            }
        }

        serverSocket->send("quit");
    }
    else
    {
        cout << endl << "Could not connect to server" << endl << endl;
    }
}