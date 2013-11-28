/* utils.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 18 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   random utiltiies
*/



/******************************************************************************/
/* SLICK CHECK                                                                */
/******************************************************************************/

#define SLICK_CHECK_ERRNO(pred,msg)                             \
    do {                                                        \
        if (!(pred))                                            \
            throw std::string(msg) + ": " + strerror(errno);    \
    } while(false);                                             \


/******************************************************************************/
/* THREAD ID                                                                  */
/******************************************************************************/

// Thread local storage.
#define slickTls __thread __attribute__(( tls_model("initial-exec") ))

namespace slick {

size_t threadId();


/******************************************************************************/
/* BIT OPS                                                                    */
/******************************************************************************/

inline size_t clz(unsigned x)           { return __builtin_clz(x); }
inline size_t clz(unsigned long x)      { return __builtin_clzl(x); }
inline size_t clz(unsigned long long x) { return __builtin_clzll(x); }


/******************************************************************************/
/* WALL                                                                       */
/******************************************************************************/

/** Plain old boring time taken from the kernel using a syscall. */
struct Wall
{
    typedef double ClockT;
    enum { CanWrap = false };

    ClockT operator() () const locklessAlwaysInline
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
struct Monotonic
{
    typedef double ClockT;
    enum { CanWrap = false };

    ClockT operator() () const locklessAlwaysInline
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

    double elapsed() const locklessAlwaysInline
    {
        return Clock::diff(start, clock());
    }

    double reset() locklessAlwaysInline
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


} // namespace slick
