#include "socket.h"

#ifdef __linux__
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#elif _WIN32
#pragma comment(lib,"Ws2_32.lib")
#include <Windows.h>
#endif

#include <cstring>
#include <stdexcept>

using namespace std;

Socket::~Socket()
{
    close();
}

void Socket::close()
{
    if (m_state != SOCKET_STATE::DISCONNECTED)
    {
        if (m_socketDescriptor > 0)
        {
#ifdef __linux__
            ::close(m_socketDescriptor);
#elif _WIN32
            ::closesocket(m_socketDescriptor);
#endif
        }
        m_state = SOCKET_STATE::DISCONNECTED;
    }
}

bool Socket::create(SOCKET_TYPE type)
{
    if (type == SOCKET_TYPE::TCP)
    {
        m_socketDescriptor = socket(PF_INET, SOCK_STREAM, 0);
    }
    else if (type == SOCKET_TYPE::UDP)
    {
        m_socketDescriptor = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }

    if (m_socketDescriptor < 0)
    {
        return false;
    }

    m_socketType = type;

    // Prefer low latency
    setSocketOption(SOCKET_OPTION::TCP_DISABLE_NAGLE, 1);

    return true;
}

int Socket::getLastSocketError()
{
    return getSocketOption(SOCKET_OPTION::GET_ERROR_AND_CLEAR);
}

bool Socket::isConnectionLost(int errorCode, size_t receiveResult)
{
    bool ret{ false };
#ifdef __linux__
    /*
        100 = Network is down
        101 = Network is unreachable
        102 = Network dropped connection on reset
        103 = Software caused connection abort
        104 = Connection reset by peer
    */
    if (errorCode >= 100 && errorCode <= 104 )
    {
        ret = true;
    }
#elif _WIN32
    if (errorCode == WSAECONNRESET || errorCode == WSAECONNABORTED)
    {
        ret = true;
    }
#endif
    if (ret == true)
    {
        m_state = SOCKET_STATE::DISCONNECTED;
    }
    return ret;
}

string Socket::getSocketErrorAsString(int errorCode)
{
    string ret;
#ifdef __linux__
    ret = strerror(errorCode);
#elif _WIN32
    HMODULE lib = ::LoadLibraryA("WSock32.dll");
    char* tempString = nullptr;

    FormatMessageA(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
        (LPCVOID)lib, errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&tempString, 0, NULL);

    if (tempString)
    {
        ret = tempString;
        LocalFree(tempString);
    }

    if (lib)
    {
        ::FreeLibrary(lib);
    }
#endif
    return ret;
}

int Socket::getCurrentThreadLastSocketError()
{
    int ret{ -1 };
#ifdef __linux__
    ret = errno;
#elif _WIN32
    ret = WSAGetLastError();
#endif
    return ret;
}

int Socket::getSocketOption(SOCKET_OPTION option)
{
    int actualOption = getSocketOptionValue(option);
    int ret{ 0 };
    socklen_t len;

#ifdef __linux__
    getsockopt(m_socketDescriptor, SOL_SOCKET, actualOption, (void*)(&ret), &len);
#elif _WIN32
    getsockopt(m_socketDescriptor, SOL_SOCKET, actualOption, (char*)(&ret), &len);
#endif

    return ret;
}

void Socket::setSocketOption(SOCKET_OPTION option, int value)
{
    int actualOption = getSocketOptionValue(option);

    if (!actualOption)
    {
        // Even though called ,not supported on this system, for ex QUICK_ACK for Windows
        return;
    }

    int actualValue = value;
#if __linux
    setsockopt(m_socketDescriptor, SOL_SOCKET, actualOption, &actualValue, sizeof actualValue);
#elif _WIN32
    setsockopt(m_socketDescriptor, SOL_SOCKET, actualOption, (char *)&actualValue, sizeof actualValue);
#endif
}

bool Socket::listen()
{
    int result = ::listen(m_socketDescriptor, m_pendingConnectionsQueueSize);

    if (result != 0)
    {
        return false;
    }

    m_state = SOCKET_STATE::LISTENING;
    return true;
}

bool Socket::connect(const string& address, int port)
{
    initialise(address, port);

    if (::connect(m_socketDescriptor, (struct sockaddr*)&m_socketAddress, sizeof(m_socketAddress)) != 0)
    {
        return false;
    }

    m_state = SOCKET_STATE::CONNECTED;
    return true;
}

bool Socket::connect(const string& address, int port, int timeout)
{
    setBlockingMode(false);

    bool success{ false };

    success = connect(address, port);

    if (success == false)
    {
        success = select(true, true, timeout);
    }

    setBlockingMode(true);

    if (success == false)
    {
        return false;
    }

    return true;
}

bool Socket::select(bool read, bool write, long timeout)
{
    fd_set writeSet;
    fd_set readSet;
    fd_set* readSetPtr{nullptr};
    fd_set* writeSetPtr{nullptr};

    if (write)
    {
        FD_ZERO(&writeSet);
        FD_SET(m_socketDescriptor, &writeSet);
        writeSetPtr = &writeSet;
    }

    if (read)
    {
        FD_ZERO(&readSet);
        FD_SET(m_socketDescriptor, &readSet);
        readSetPtr = &readSet;
    }

    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    // First arg ignored in Windows
    if (::select(m_socketDescriptor + 1, readSetPtr, writeSetPtr, nullptr, &tv) <= 0)
    {
        return false;
    }

    /*
    IMPORTANT : CALLING THE ONE UNDER MSVC CAUSING MEMORY CORRUPTION
    if (getLastSocketError() != 0)
    {
        return false;
    }
    */

    return true;
}

bool Socket::bind(const string& address, int port)
{
    initialise(address, port);
    int result = ::bind(m_socketDescriptor, (struct sockaddr*)&m_socketAddress, sizeof(m_socketAddress));

    if (result != 0)
    {
        return false;
    }

    m_state = SOCKET_STATE::BOUND;

    return true;
}

void Socket::setSocketAddress(struct sockaddr_in* addr, const std::string& address, int port)
{
    memset(addr, 0, sizeof(sockaddr_in));
    addr->sin_family = PF_INET;
    addr->sin_port = htons(port);

    if (address.size() > 0)
    {
        if (getAddressInfo(address.c_str(), &(addr->sin_addr)) != 0)
        {
            inet_pton(PF_INET, address.c_str(), &(addr->sin_addr));
        }
    }
    else
    {
        addr->sin_addr.s_addr = INADDR_ANY;
    }
}

void Socket::initialise(const std::string& address, int port)
{
    m_port = port;
    m_address = address;
    setSocketAddress(&m_socketAddress, address, port);
}

void Socket::initialise(int socketDescriptor, struct sockaddr_in* socketAddress)
{
    m_socketDescriptor = socketDescriptor;
    char ip[50];
#ifdef __linux__
    inet_ntop(PF_INET, (struct in_addr*)&(socketAddress->sin_addr.s_addr), ip, sizeof(ip) - 1);
#elif _WIN32
    InetNtopA(PF_INET, (struct in_addr*)&(socketAddress->sin_addr.s_addr), ip, sizeof(ip) - 1);
#endif
    m_address = ip;
    m_port = ntohs(socketAddress->sin_port);
}

Socket* Socket::accept(int timeout)
{
    if (m_state != SOCKET_STATE::LISTENING && m_state != SOCKET_STATE::ACCEPTED)
    {
        return nullptr;
    }

    setBlockingMode(false);

    bool success{ true };
    int peerSocketDesc{ -1 };
    struct sockaddr_in address;
    socklen_t len = sizeof(address);

    memset(&address, 0, sizeof(address));
    success = select(true, false, timeout);

    if (success)
    {

        peerSocketDesc = ::accept(m_socketDescriptor, (struct sockaddr*)&address, &len);

        if (peerSocketDesc<0)
        {
            success = false;
        }
    }

    setBlockingMode(true);

    if (!success)
    {
        return nullptr;
    }

    Socket* peerSocket = new Socket;
    peerSocket->initialise(peerSocketDesc, &address);
    peerSocket->m_state = SOCKET_STATE::CONNECTED;

    m_state = SOCKET_STATE::ACCEPTED;

    return peerSocket;
}

int Socket::getAddressInfo(const char* hostname, struct in_addr* socketAddress)
{
    struct addrinfo *res{ nullptr };

    int result = getaddrinfo(hostname, nullptr, nullptr, &res);

    if (result == 0)
    {
        memcpy(socketAddress, &((struct sockaddr_in *) res->ai_addr)->sin_addr, sizeof(struct in_addr));
        freeaddrinfo(res);
    }

    return result;
}

int Socket::getSocketOptionValue(SOCKET_OPTION option)
{
    int ret{ 0 };

    switch (option)
    {
        case SOCKET_OPTION::GET_ERROR_AND_CLEAR:
            ret = SO_ERROR;
            break;
        case SOCKET_OPTION::REUSE_ADDRESS:
            ret = SO_REUSEADDR;
            break;
        case SOCKET_OPTION::EXCLUSIVE_ADDRESS:
            ret = SO_REUSEADDR;
            break;
        case SOCKET_OPTION::REUSE_PORT:
            #ifdef SO_REUSEPORT
            ret = SO_REUSEPORT;
            #endif
            break;
        case SOCKET_OPTION::RECEIVE_BUFFER_SIZE:
            ret = SO_RCVBUF;
            break;
        case SOCKET_OPTION::RECEIVE_BUFFER_TIMEOUT:
            ret = SO_RCVTIMEO;
            break;
        case SOCKET_OPTION::SEND_BUFFER_SIZE:
            ret = SO_SNDBUF;
            break;
        case SOCKET_OPTION::SEND_BUFFER_TIMEOUT:
            ret = SO_SNDTIMEO;
            break;
        case SOCKET_OPTION::TCP_DISABLE_NAGLE:
            ret = TCP_NODELAY;
            break;
        case SOCKET_OPTION::TCP_ENABLE_QUICKACK:
            #ifdef TCP_QUICKACK
            ret = TCP_QUICKACK;
            #endif
            break;
        case SOCKET_OPTION::TCP_ENABLE_CORK:
            #ifdef TCP_CORK
            ret = TCP_CORK;
            #endif
            break;
        case SOCKET_OPTION::SOCKET_PRIORITY:
            #ifdef SO_PRIORITY
            ret = SO_PRIORITY;
            #endif
            break;
        case SOCKET_OPTION::POLLING_INTERVAL:
            #ifdef SO_BUSY_POLL
            ret = SO_BUSY_POLL;
            #endif
            break;
        default:
            throw std::runtime_error("Non supported socket option");
    }

    return ret;
}

void Socket::setBlockingMode(bool blockingMode)
{
#if __linux__
    long arg = fcntl(m_socketDescriptor, F_GETFL, NULL);

    if (blockingMode)
    {
        arg &= (~O_NONBLOCK);
    }
    else
    {
        arg |= O_NONBLOCK;
    }

    fcntl(m_socketDescriptor, F_SETFL, arg);
#elif _WIN32
    u_long iMode{0};

    if (!blockingMode)
    {
        iMode = 1;
    }

   ioctlsocket(m_socketDescriptor, FIONBIO, &iMode);
#endif
}