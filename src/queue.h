/* queue.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 30 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Lockfree queue specialized for our endpoints.
*/

#pragma once

#include "utils.h"

#include <array>
#include <atomic>

namespace slick {


/******************************************************************************/
/* QUEUE                                                                      */
/******************************************************************************/

/** Single-Consumer Multi-Producer queue with signaling. */
template<typename T, size_t Size>
struct Queue
{
    Queue() { d.cursors = 0; }

    int fd() const { notify.fd(); }

    constexpr size_t capacity() const { return Size; }

    size_t size() const
    {
        uint64_t all = d.cursors;
        return wpos(old) - rpos(old);
    }

    bool empty() const
    {
        uint64_t all = d.cursors;
        return wpos(old) == rpos(old);
    }

    T&& pop()
    {
        std::assert(!empty());
        return std::move(queue[d.split.read++ % Size]);
    }

    template<typename Val>
    void push(Val&& val)
    {
        std::lock_guard<SpinLock> guard(lock);

        uint64_t old = d.cursors;
        std::assert(wpos(old) - rpos(old) < Size);

        queue[wpos(old) % Size] = std::forward<Val>(val);
        d.split.write++; // should act as a release barier.
    }

private:

    uint32_t rpos(uint64_t all) const { return all; }
    uint32_t wpos(uint64_t all) const { return all >> 32; }

    union {
        struct {
            uint32_t read;
            std::atomic<uint32_t> write;
        } split;
        std::atomic<uint64_t> cursors;
    } d;

    SpinLock lock;
    std::array<T, Size> queue;
};


} // slick
