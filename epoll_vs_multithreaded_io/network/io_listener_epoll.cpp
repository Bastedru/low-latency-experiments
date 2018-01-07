#include "io_listener_epoll.h"

IOListenerEpoll::IOListenerEpoll():m_epollMode{ EPOLL_MODE::EDGE_TRIGGERED },
m_epollDescriptor{ -1 }, m_epollEvents{ nullptr }, m_epollTimeout{0}
{
}

IOListenerEpoll::~IOListenerEpoll()
{
    stop();
}

void IOListenerEpoll::start()
{
    m_epollDescriptor = epoll_create1(0);
    m_epollEvents = new struct epoll_event[MAX_POLL_EVENTS];
}

void IOListenerEpoll::stop()
{
    if (m_epollEvents)
    {
        delete[] m_epollEvents;
        m_epollEvents = nullptr; // delete[] does not null
    }
}

void IOListenerEpoll::setTimeout(long microSeconds)
{
    m_epollTimeout = microSeconds / 1000;
}

void IOListenerEpoll::addFileDescriptor(int fd)
{
    struct epoll_event epollSocketDescriptor;
    epollSocketDescriptor.data.fd = fd;

    epollSocketDescriptor.events = EPOLLIN;

    if(m_epollMode == EPOLL_MODE::EDGE_TRIGGERED)
    {
        epollSocketDescriptor.events |= EPOLLET;
    }

    epoll_ctl(m_epollDescriptor, EPOLL_CTL_ADD, fd, &epollSocketDescriptor);
}

void IOListenerEpoll::removeFileDescriptor(int fd)
{
    struct epoll_event epollSocketDescriptor;
    epollSocketDescriptor.data.fd = fd;
    epollSocketDescriptor.events = EPOLLIN;

    if(m_epollMode == EPOLL_MODE::EDGE_TRIGGERED)
    {
        epollSocketDescriptor.events |= EPOLLET;
    }

    epoll_ctl(m_epollDescriptor, EPOLL_CTL_DEL, fd, &epollSocketDescriptor);
}

int IOListenerEpoll::getNumberOfReadyFileDescriptors()
{
    int result{ -1 };

    result = ::epoll_wait(m_epollDescriptor, m_epollEvents, MAX_POLL_EVENTS, m_epollTimeout);

    return result;
}

int IOListenerEpoll::getReadyFileDescriptor(int index)
{
    int ret{ -1 };
    ret = m_epollEvents[index].data.fd;
    return ret;
}