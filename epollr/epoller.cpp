#include "epoller.h"

Epoller::Epoller(int maxEvents):ep_fd(epoll_create1(MAXNUMBER/2)), ep_events(maxEvents)
{
    assert(ep_fd >= 0 && ep_events.size() > 0);
}

Epoller::~Epoller()
{
    close(ep_fd);
}


bool Epoller::addFd(int fd, uint32_t events)
{
    if (fd < 0)
    {
        return false;
    }

    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;

    return (0 == epoll_ctl(ep_fd, EPOLL_CTL_ADD,fd, &ev));
}

bool Epoller::modifyFd(int fd, uint32_t events)
{
    if (fd < 0)
        return false;

    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;

    return (0 == epoll_ctl(ep_fd, EPOLL_CTL_MOD, fd, &ev));
}

bool Epoller::deleteFd(int fd)
{
    if (fd < 0)
        return false;

    epoll_event ev= {0};
    return (0 == epoll_ctl(ep_fd, EPOLL_CTL_DEL, fd, &ev));
}

int Epoller::waitTime(int timeoutMs)
{
    return epoll_wait(ep_fd, &ep_events[0], static_cast<int>(ep_events.size()),  timeoutMs);
}

int Epoller::getFdOfEvent(size_t i) const{
    assert(i >= 0 && i < ep_events.size());
    return ep_events[i].data.fd;
}

uint32_t Epoller::getEvent(size_t i) const
{
    assert(i >= 0 && i < ep_events.size());
    return ep_events[i].events;
}