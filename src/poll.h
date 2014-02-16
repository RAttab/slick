/* poll.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 28 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Epoll wrapper
*/

#pragma once

#include "lockless/tls.h"

#include <thread>
#include <atomic>
#include <functional>
#include <cassert>
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
/* START POLLING                                                              */
/******************************************************************************/

typedef std::function<void()> StartPollingFn;

namespace details {

template<typename T>
struct HasStartPolling
{
    template<typename U> static std::true_type test(decltype(&U::startPolling));
    template<typename U> static std::false_type test(...);

    typedef decltype(test<T>(0)) type;
    static constexpr bool value = std::is_same<type, std::true_type>::value;
};

template<typename T>
StartPollingFn startPollingFn(T& object, std::true_type)
{
    return std::bind(&T::startPolling, &object);
}

template<typename T>
StartPollingFn startPollingFn(T&, std::false_type)
{
    return StartPollingFn();
}

} // namespace details


template<typename T>
StartPollingFn startPollingFn(T& object)
{
    return details::startPollingFn(
            object, typename details::HasStartPolling<T>::type());
}


/******************************************************************************/
/* STOP POLLING                                                               */
/******************************************************************************/

typedef std::function<void()> StopPollingFn;

namespace details {

template<typename T>
struct HasStopPolling
{
    template<typename U> static std::true_type test(decltype(&U::stopPolling));
    template<typename U> static std::false_type test(...);

    typedef decltype(test<T>(0)) type;
    static constexpr bool value = std::is_same<type, std::true_type>::value;
};

template<typename T>
StopPollingFn stopPollingFn(T& object, std::true_type)
{
    return std::bind(&T::stopPolling, &object);
}

template<typename T>
StopPollingFn stopPollingFn(T&, std::false_type)
{
    return StopPollingFn();
}

} // namespace details


template<typename T>
StopPollingFn stopPollingFn(T& object)
{
    return details::stopPollingFn(
            object, typename details::HasStopPolling<T>::type());
}


/******************************************************************************/
/* SOURCE POLLER                                                              */
/******************************************************************************/

struct SourcePoller
{
    SourcePoller() {}
    SourcePoller(SourcePoller&&) = default;
    SourcePoller(const SourcePoller&) = delete;
    SourcePoller& operator=(SourcePoller&&) = default;
    SourcePoller& operator=(const SourcePoller&) = delete;


    typedef std::function<void()> SourceFn;

    int fd() const { return poller.fd(); }
    void poll(size_t timeout = 0);
    void startPolling();
    void stopPolling();

    template<typename T>
    void add(T& source)
    {
        T* pSource = &source;

        auto sourceFn = [=] { pSource->poll(); };
        auto startFn = startPollingFn(source);
        auto stopFn = stopPollingFn(source);

        add(source.fd(), sourceFn, startFn, stopFn);
    }

    void add(int fd,
            const SourceFn& sourceFn,
            const StartPollingFn& startFn = {},
            const StopPollingFn& stopFn = {});

    template<typename T>
    void del(T& source) { del(source.fd()); }
    void del(int fd);

private:
    Epoll poller;

    struct Callbacks
    {
        SourceFn sourceFn;
        StartPollingFn startFn;
        StopPollingFn stopFn;
    };
    std::unordered_map<int, Callbacks> sources;
};


/******************************************************************************/
/* POLL THREAD DETECTOR                                                       */
/******************************************************************************/

/** Need something a bit less flimsy since it's open to a race between the first
    call to poll (which will in turn call set) and any potential off thread
    operations.
 */
struct IsPollThread
{
    IsPollThread() : pollThread(0) {}

    void set() { pollThread = lockless::threadId(); }
    void unset() { pollThread = 0; }

    bool isPolling() { return pollThread; }
    bool operator() () const
    {
        return !pollThread || pollThread == lockless::threadId();
    }

private:
    size_t pollThread;
};


/******************************************************************************/
/* THREAD AWARE POLLABLE                                                      */
/******************************************************************************/

struct ThreadAwarePollable
{
    virtual ~ThreadAwarePollable() {}

    virtual void startPolling()
    {
        isPollThread.set();
    }

    virtual void stopPolling()
    {
        isPollThread.unset();
    }

protected:
    IsPollThread isPollThread;
};


/******************************************************************************/
/* POLL THREAD                                                                */
/******************************************************************************/

// EXTREMELY simple poll thread. Mostly useful for tests.
struct PollThread : public SourcePoller
{
    PollThread() : isDone(true) {}
    ~PollThread() { join(); }

    void run();
    void join();

private:
    std::thread th;
    std::atomic<bool> isDone;
};


} // slick
