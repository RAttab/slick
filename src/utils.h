/* utils.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 18 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   random utiltiies
*/

#pragma once

#include <set>
#include <string>
#include <random>
#include <stdexcept>
#include <cstdlib>
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
/* SEQ                                                                        */
/******************************************************************************/

template<size_t...> struct Seq {};

template<size_t N, size_t... S>
struct GenSeq : public GenSeq<N-1, N-1, S...> {};

template<size_t... S>
struct GenSeq<0, S...>
{
    typedef Seq<S...> type;
};


/******************************************************************************/
/* HASH COMBINE                                                               */
/******************************************************************************/

// Boost implementation.
template<typename T>
inline void hash_combine(size_t& seed, const T& value)
{
    std::hash<T> hash;
    seed ^= hash(value) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}


/******************************************************************************/
/* GUARD                                                                      */
/******************************************************************************/

template<typename Fn>
struct GuardFn
{
    GuardFn(Fn fn) : fn(std::move(fn)) {}
    ~GuardFn() { fn(); }

private:
    Fn fn;
};

template<typename Fn>
GuardFn<Fn> guard(Fn fn) { return GuardFn<Fn>(std::move(fn)); }


/******************************************************************************/
/* PICK RANDOM                                                                */
/******************************************************************************/

template<typename It, typename Rng>
It pickRandom(It it, It last, Rng& rng)
{
    if (it == last) return last;

    size_t dist = std::distance(it, last);
    size_t n = std::uniform_int_distribution<size_t>(0, dist - 1)(rng);
    std::advance(it, n);
    return it;
}

template<typename T, typename It, typename Rng>
std::set<T> pickRandom(It first, It last, size_t n, Rng& rng)
{
    if (first == last) return {};

    std::set<T> result;
    while (result.size() < n) {
        auto it = pickRandom(first, last, rng);
        if (it == last) break;
        result.insert(*it);
    }
    return std::move(result);
}



} // namespace slick
