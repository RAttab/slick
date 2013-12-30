/* defer.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 27 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Operation deferal mechanism.
*/

#pragma once

#include "queue.h"
#include "utils.h"

#include <tuple>
#include <functional>


namespace slick {


/******************************************************************************/
/* INVOKE                                                                     */
/******************************************************************************/

namespace details {

template<typename Fn, typename... Args, size_t... S>
void invoke(const Fn& fn, std::tuple<Args...>& tuple, Seq<S...>)
{
    fn(std::move(std::get<S>(tuple))...);
}

template<typename Fn, typename... Args>
void invoke(const Fn& fn, std::tuple<Args...>& tuple)
{
    invoke(fn, tuple, typename GenSeq<sizeof...(Args)>::type());
}

} // namespace details


/******************************************************************************/
/* DEFER                                                                      */
/******************************************************************************/

template<size_t QueueSize, typename... Items>
struct Defer
{
    typedef std::function<void(Items&&...)> OperationFn;
    OperationFn onOperation;

    int fd() const { return notify.fd(); }

    void poll(size_t cap = 0)
    {
        assert(onOperation);

        while (notify.poll());

        for (size_t i = 0; !queue.empty() && (!cap || i < cap); ++i) {
            auto op = queue.pop();
            details::invoke(onOperation, op);
        }

        if (!queue.empty()) notify.signal();
    }

    template<typename... Args>
    void defer(Args&&... args)
    {
        auto op = std::make_tuple(std::forward<Args>(args)...);

        while (!queue.push(std::move(op)));
        notify.signal();
    }

    template<typename... Args>
    bool tryDefer(Args&&... args)
    {
        auto op = std::make_tuple(std::forward<Args>(args)...);

        if (!queue.push(std::move(op)))
            return false;

        notify.signal();
        return true;
    }

private:

    Queue<std::tuple<Items...>, QueueSize> queue;
    Notify notify;
};

} // slick
