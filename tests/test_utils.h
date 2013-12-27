/* test_utils.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 07 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Test utilities.
*/

#pragma once

#include <signal.h>
#include <unistd.h>

namespace slick {


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
