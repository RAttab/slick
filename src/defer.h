/* defer.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 27 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Operation deferal mechanism.
*/

#pragma once

#include "queue.h"

namespace slick {


/******************************************************************************/
/* DEFER OP                                                                   */
/******************************************************************************/

template<typename Operation, size_t QueueSize = 1ULL << 6>
struct Defer
{
    typedef std::function<void(Operation&&)> OperationFn;
    OperationFn onOperation;

    void defer(Operation&& op)
    {
        while (!operations.push(std::move(op)));
        notify.signal();
    }

    template<typename... Args>
    void defer(Args&&... args)
    {
        defer(Operation(std::forward<Args>(args)...));
    }


    bool tryDefer(Operation&& op)
    {
        if (!operations.push(std::move(op)))
            return false;

        notify.signal();
        return true;
    }

    template<typename... Args>
    bool tryDefer(Args&&... args)
    {
        return tryDefer(Operation(std::forward<Args>(args)...));
    }


    int fd() const { return notify.fd(); }
    void poll(size_t cap = 0)
    {
        assert(onOperation);

        while (notify.poll());

        for (size_t i = 0; !operations.empty() && (!cap || i < cap); ++i) {
            Operation op = operations.pop();
            onOperation(std::move(op));
        }

        if (!operations.empty()) notify.signal();
    }

private:
    Queue<Operation, QueueSize> operations;
    Notify notify;
};

} // slick
