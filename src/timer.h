/* timer.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 27 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Timer thingy.
*/

#pragma once

#include <functional>
#include <cstdint>

namespace slick {


/******************************************************************************/
/* TIMER                                                                      */
/******************************************************************************/

struct Timer
{
    Timer(double delay, double init = 0);
    ~Timer();

    typedef std::function<void(uint64_t count)> TimerFn;
    TimerFn onTimer;

    int fd() const { return fd_; }
    void poll();

    void setDelay(double delay, double init = 0);

private:
    int fd_;
};

} // slick
