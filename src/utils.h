/* utils.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 18 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   random utiltiies
*/

#pragma once

#include <chrono>
#include <thread>
#include <atomic>
#include <string>
#include <stdexcept>
#include <cstdlib>
#include <time.h>
#include <string.h>
#include <unistd.h>

namespace slick {


/******************************************************************************/
/* STATIC ASSERT                                                              */
/******************************************************************************/

#define slickStaticAssert(x) static_assert(x, #x)


/******************************************************************************/
/* SLICK CHECK                                                                */
/******************************************************************************/

inline std::string checkErrnoString(int err, const std::string& msg)
{
    return msg + ": " + strerror(err);
}

inline std::string checkErrnoString(const std::string& msg)
{
    return checkErrnoString(errno, msg);
}

#define SLICK_CHECK_ERRNO(pred,msg)                             \
    do {                                                        \
        if (!(pred))                                            \
            throw std::logic_error(checkErrnoString(msg));      \
    } while(false)                                              \


/******************************************************************************/
/* TLS                                                                        */
/******************************************************************************/

size_t threadId();

// Thread local storage.
#define slickTls __thread __attribute__(( tls_model("initial-exec") ))


/******************************************************************************/
/* BIT OPS                                                                    */
/******************************************************************************/

inline size_t clz(unsigned x)           { return __builtin_clz(x); }
inline size_t clz(unsigned long x)      { return __builtin_clzl(x); }
inline size_t clz(unsigned long long x) { return __builtin_clzll(x); }


/******************************************************************************/
/* SLEEP                                                                      */
/******************************************************************************/

inline void sleep(size_t ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

/******************************************************************************/
/* WALL                                                                       */
/******************************************************************************/

/** Plain old boring time taken from the kernel using a syscall. */
extern struct Wall
{
    typedef double ClockT;
    enum { CanWrap = false };

    ClockT operator() () const
    {
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) < 0)
            return -1;

        return ts.tv_sec + (ts.tv_nsec * 0.0000000001);
    }

    static constexpr double toSec(ClockT t) { return t; }
    static constexpr ClockT diff(ClockT first, ClockT second)
    {
        return second - first;
    }

} wall;


/******************************************************************************/
/* MONOTONIC                                                                  */
/******************************************************************************/

/** Monotonic uses rdtsc except that it also protects us from the process/thread
    migrating to other CPU.

    It doesn't take into account the various power-state of the CPU which could
    skew the results. The only way to account for this is to count cycles or to
    structure the test such that there's enough warmup to get the CPU to wake
    up. Unfortunately, warmups are not always feasable and accessing performance
    counters requires (I think) a kernel module to enable them in userland.

    Sadly, we live in an imperfect world.
 */
extern struct Monotonic
{
    typedef double ClockT;
    enum { CanWrap = false };

    ClockT operator() () const
    {
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) < 0)
            return -1;

        return ts.tv_sec + (ts.tv_nsec * 0.0000000001);
    }

    static constexpr double toSec(ClockT t) { return t; }
    static constexpr ClockT diff(ClockT first, ClockT second)
    {
        return second - first;
    }

} monotonic;


/******************************************************************************/
/* TIMER                                                                      */
/******************************************************************************/

template<typename Clock>
struct Timer
{
    typedef typename Clock::ClockT ClockT;

    Timer() : start(clock()) {}

    double elapsed() const
    {
        return Clock::diff(start, clock());
    }

    double reset()
    {
        ClockT end = clock();
        ClockT elapsed = Clock::diff(start, end);
        start = end;
        return Clock::toSec(elapsed);
    }

private:
    Clock clock;
    ClockT start;
};


/******************************************************************************/
/* SPIN LOCK                                                                  */
/******************************************************************************/

struct SpinLock
{
    SpinLock() : val(0) {}

    SpinLock(SpinLock&&) = delete;
    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(SpinLock&&) = delete;
    SpinLock& operator=(const SpinLock&) = delete;

    void lock()
    {
        size_t oldVal;
        while((oldVal = val) || !val.compare_exchange_weak(oldVal, 1));
    }

    bool tryLock()
    {
        size_t oldVal = val;
        return !oldVal && val.compare_exchange_strong(oldVal, 1);
    }

    void unlock() { val.store(0); }

private:
    std::atomic<size_t> val;
};


/******************************************************************************/
/* FORK                                                                       */
/******************************************************************************/

struct Fork
{
    Fork();
    ~Fork();

    bool isParent() const { return pid; }
    void killChild();

private:
    int pid;
    bool killed;
};

void disableBoostTestSignalHandler();

} // namespace slick
