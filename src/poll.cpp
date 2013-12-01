/* poll.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 28 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Epoll wrapper
*/

#include "poll.h"
#include "utils.h"

#include <cstring>
#include <sys/epoll.h>

namespace slick {

/******************************************************************************/
/* EPOLL                                                                      */
/******************************************************************************/

Epoll::
Epoll() : nextEvent(0), numEvents(0)
{
    fd_ = epoll_create(1);
    SLICK_CHECK_ERRNO(fd_ != -1, "epoll_create");
}

Epoll::
~Epoll()
{
    close(fd_);
}

void
Epoll::
add(int fd, int flags)
{
    struct epoll_event ev;
    std::memset(&ev, 0, sizeof ev);

    ev.events = flags;

    int ret = epoll_ctl(fd_, EPOLL_CTL_ADD, fd, &ev);
    SLICK_CHECK_ERRNO(ret != -1, "epoll_ctl.add");
}


void
Epoll::
del(int fd)
{
    int ret = epoll_ctl(fd_, EPOLL_CTL_DEL, fd, nullptr);
    SLICK_CHECK_ERRNO(ret != -1, "epoll_ctl.del");
}


struct epoll_event
Epoll::
next()
{
    while (!poll(-1));
    return events[nextEvent++];
}

bool
Epoll::
poll(int timeoutMs)
{
    while (nextEvent == numEvents) {
        int n = epoll_wait(fd_, events, MaxEvents, timeoutMs);
        if (n < 0 && errno == EINTR) continue;
        SLICK_CHECK_ERRNO(n >= 0, "epoll_wait");

        numEvents = n;
        nextEvent = 0;
        break;
    }

    return nextEvent < numEvents;
}



} // slick
