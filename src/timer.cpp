/* timer.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 27 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Timer implementation.
*/

#include "timer.h"
#include "utils.h"

#include <cassert>
#include <unistd.h>
#include <sys/timerfd.h>

namespace slick {


/******************************************************************************/
/* TIMER                                                                      */
/******************************************************************************/

Timer::
Timer(double delay, double init)
{
    int clockid = delay < 0.01 ? CLOCK_MONOTONIC : CLOCK_REALTIME;

    fd_ = timerfd_create(clockid, TFD_NONBLOCK);
    SLICK_CHECK_ERRNO(fd_ != -1, "timer.create");

    setDelay(delay, init);
}


Timer::
~Timer()
{
    if (fd_ >= 0) close(fd_);
}

void
Timer::
poll()
{

    uint64_t expirations = 0;

    ssize_t bytes = read(fd_, &expirations, sizeof(expirations));

    if (bytes == -1) {
        if (errno == EAGAIN || EWOULDBLOCK) return;
        SLICK_CHECK_ERRNO(bytes != -1, "timer.read");
    }

    assert(bytes == sizeof(expirations));
    if (onTimer) onTimer(expirations);
}

void
Timer::
setDelay(double delay, double init)
{
    if (init == 0.0) init = delay;

    auto ts = [] (double value) {
        struct timespec ts;
        ts.tv_sec = uint64_t(value);
        ts.tv_nsec = (value - ts.tv_sec) * 1000000000;
        return ts;
    };


    struct itimerspec spec = { ts(delay), ts(init) };
    int res = timerfd_settime(fd_, 0, &spec, nullptr);
    SLICK_CHECK_ERRNO(res != -1, "timer.settime");
}


} // slick
