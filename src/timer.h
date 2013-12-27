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
    Timer(double delay);
    ~Timer();

    typedef std::function<void(uint64_t count)> TimerFn;
    TimerFn onTimer;

    int fd() const { return fd_; }
    void poll();

private:

    /** Could make this public but that introduces concurrency issues. */
    void setDelay(double delay);

    int fd_;
};

} // slick
