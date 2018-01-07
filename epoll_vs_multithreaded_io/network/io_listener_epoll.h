#ifndef __IO_LISTENER_EPOLL__
#define __IO_LISTENER_EPOLL__

#include <sys/time.h>
#include <sys/epoll.h>
#define MAX_POLL_EVENTS 64

enum class EPOLL_MODE
{
    LEVEL_TRIGGERED,
    EDGE_TRIGGERED
};

class IOListenerEpoll
{
    public:
        IOListenerEpoll();
        ~IOListenerEpoll();
        void start();
        void stop();
        void setTimeout(long microSeconds);
        void addFileDescriptor(int fd);
        void removeFileDescriptor(int fd);
        int getNumberOfReadyFileDescriptors();
        int getReadyFileDescriptor(int index);
        void setEpollMode(EPOLL_MODE mode) { m_epollMode = mode; }
    private:
        EPOLL_MODE m_epollMode;
        int m_epollDescriptor;
        struct epoll_event* m_epollEvents;
        int m_epollTimeout;
};

#endif