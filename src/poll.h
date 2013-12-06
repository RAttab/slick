/* poll.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 28 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Epoll wrapper
*/

#pragma once

#include <cassert>

#include <functional>
#include <unordered_map>
#include <sys/epoll.h>

namespace slick {

/******************************************************************************/
/* EPOLL                                                                      */
/******************************************************************************/

struct Epoll
{
    Epoll();
    ~Epoll();

    Epoll(const Epoll&) = delete;
    Epoll& operator=(const Epoll&) = delete;

    void add(int fd, int flags = EPOLLIN);
    void del(int fd);
    struct epoll_event next();
    bool poll(int timeoutMs = 0);

    int fd() const { return fd_; }

private:
    int fd_;

    enum { MaxEvents = 1U << 4 };
    struct epoll_event events[MaxEvents];
    size_t nextEvent;
    size_t numEvents;
};


/******************************************************************************/
/* SOURCE POLLER                                                              */
/******************************************************************************/

struct SourcePoller
{
    SourcePoller() {}
    SourcePoller(const SourcePoller&) = delete;
    SourcePoller& operator=(const SourcePoller&) = delete;
    SourcePoller(SourcePoller&&) = default;
    SourcePoller& operator=(SourcePoller&&) = default;


    typedef std::function<void()> SourceFn;

    int fd() const { return poller.fd(); }

    template<typename T>
    void add(T& source)
    {
        T* pSource = &source;
        add(source.fd(), [=] { pSource->poll(); });
    }
    void add(int fd, const SourceFn& fn);

    template<typename T>
    void del(T& source) { del(source.fd()); }
    void del(int fd);

    void poll(size_t timeout = 0);

private:
    Epoll poller;
    std::unordered_map<int, SourceFn> sources;
};


} // slick
