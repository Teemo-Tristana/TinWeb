#ifndef EPOLLER_H
#define EPOLLER_H

#include <vector>


#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <sys/epoll.h>

const int MAXNUMBER = 1024;

class Epoller{
    public:
        explicit Epoller(int maxEvent = 1024);

        ~Epoller();

        bool addFd(int fd, uint32_t events);

        bool modifyFd(int fd, uint32_t events);

        bool deleteFd(int fd);

        int waitTime(int timeout = -1);

        int getFdOfEvent(size_t i) const;

        uint32_t getEvent(size_t i) const;

    private:
        int ep_fd;

        std::vector<struct epoll_event> ep_events;
};


#endif