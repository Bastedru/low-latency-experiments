#ifndef __IO_LISTENER_SELECT__
#define __IO_LISTENER_SELECT__

#ifdef __linux__
#include <sys/select.h>
#include <sys/time.h>
#elif _WIN32
#include <Ws2tcpip.h>
#endif

class IOListenerSelect
{
    public:
        IOListenerSelect();
        ~IOListenerSelect();
        void setTimeout(long microSeconds);
        void setFileDescriptor(int fd);
        void clearFileDescriptor(int fd);
        int eventReady(int maxSocketDescriptor);
        bool isFileDescriptorReady(int fd);
    private:
        struct timeval m_timeout;
        fd_set m_clientsReadSet;
};

#endif