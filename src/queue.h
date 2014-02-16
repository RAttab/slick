/* queue.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 30 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Partially lock-free queue specialized for the defer mechanism.
*/

#pragma once

#include "utils.h"
#include "lockless/lock.h"

#include <array>
#include <atomic>
#include <utility>
#include <mutex>
#include <cassert>

namespace slick {


/******************************************************************************/
/* QUEUE                                                                      */
/******************************************************************************/

template<typename T, size_t Size>
struct Queue
{
    Queue() { d.cursors = 0; }

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    constexpr size_t capacity() const { return Size; }

    size_t size() const
    {
        uint64_t all = d.cursors;
        return wpos(all) - rpos(all);
    }

    bool empty() const
    {
        uint64_t all = d.cursors;
        return wpos(all) == rpos(all);
    }

    T pop()
    {
        assert(!empty());

        uint32_t pos = d.split.read % Size;
        T val = std::move(queue[pos]);

        d.split.read++;
        return std::move(val);
    }

    template<typename Val>
    bool push(Val&& val)
    {
        std::lock_guard<Lock> guard(lock);

        uint64_t old = d.cursors;
        if (wpos(old) - rpos(old) == Size) return false;

        queue[wpos(old) % Size] = std::forward<Val>(val);
        d.split.write++; // should act as a release barrier.

        return true;
    }

private:

    uint32_t rpos(uint64_t all) const { return all; }
    uint32_t wpos(uint64_t all) const { return all >> 32; }

    union {
        struct {
            std::atomic<uint32_t> read;
            std::atomic<uint32_t> write;
        } split;
        std::atomic<uint64_t> cursors;
    } d;

    typedef lockless::UnfairLock Lock;

    Lock lock;
    std::array<T, Size> queue;
};


} // slick
