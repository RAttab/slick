/* test_utils.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 07 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Test utilities.
*/

#pragma once

#include "utils.h"
#include "socket.h"
#include "lockless/tm.h"

#include <unordered_set>
#include <signal.h>
#include <unistd.h>

namespace slick {


/******************************************************************************/
/* PORT                                                                       */
/******************************************************************************/

namespace {

Port allocatePort(PortRange range)
{
    static std::unordered_set<Port> inUse;

    // Need a little bit of random to avoid weird errors.
    size_t offset = lockless::rdtsc();

    for (size_t i = 0; i < range.size(); ++i) {
        Port port = (i + offset) % range.size() + range.first;
        if (inUse.insert(port).second) return port;
    }

    return 0;
}

}

inline Port allocatePort(Port first = 20000, Port last = 30000)
{
    return allocatePort({ first, last });
}

/******************************************************************************/
/* BOOST                                                                      */
/******************************************************************************/

void disableBoostTestSignalHandler()
{
    auto ret = signal(SIGCHLD, SIG_DFL);
    SLICK_CHECK_ERRNO(ret != SIG_ERR, "disableBoostTestSignalHandler.signal");
}


/******************************************************************************/
/* FORK                                                                       */
/******************************************************************************/

struct Fork
{
    Fork() : pid(fork()), killed(false)
    {
        SLICK_CHECK_ERRNO(pid >= 0, "Fork.fork");
    }

    ~Fork()
    {
        if (!killed && isParent()) killChild();
    }

    bool isParent() const { return pid; }

    void killChild()
    {
        assert(!killed);

        int ret = kill(pid, SIGKILL);
        SLICK_CHECK_ERRNO(!ret, "Fork.kill");

        killed = true;
    }

private:
    int pid;
    bool killed;
};

} // namespace slick
