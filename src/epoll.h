/* epoll.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 28 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Epoll wrapper
*/

#pragma once

namespace slick {


/******************************************************************************/
/* EPOLL                                                                      */
/******************************************************************************/

struct Epoll
{
    Epoll();
    ~Epoll();

    void add(int fd, int flags = EPOLLIN);
    void del(int fd);
    struct epoll_event next();
    bool poll(int timeoutMs = 0);

    int fd() { return fd_; }

private:
    int fd_;

    enum { MaxEvents = 10 };
    struct epoll_event events[MaxEvents];
    size_t nextEvent;
    size_t numEvents;
};

} // slick
