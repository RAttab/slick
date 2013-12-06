/* utils.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 23 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Utilties for slick network
*/

#include "utils.h"

#include <atomic>
#include <cassert>
#include <signal.h>
#include <unistd.h>

namespace slick {


/******************************************************************************/
/* TLS                                                                        */
/******************************************************************************/

namespace {

std::atomic<size_t> nextThreadId(0);
slickTls size_t myThreadId = 0;

} // namespace anonymous

size_t threadId()
{
    if (!myThreadId) myThreadId = ++nextThreadId;
    return myThreadId;
}


/******************************************************************************/
/* FORK                                                                       */
/******************************************************************************/

Fork::
Fork() : pid(fork()), killed(false)
{
    SLICK_CHECK_ERRNO(pid >= 0, "Fork.fork");
}

Fork::
~Fork()
{
    if (!killed && isParent()) killChild();
}

void
Fork::
killChild()
{
    assert(!killed);

    int ret = kill(pid, SIGKILL);
    SLICK_CHECK_ERRNO(!ret, "Fork.kill");

    killed = true;
}

void disableBoostTestSignalHandler()
{
    auto ret = signal(SIGCHLD, SIG_DFL);
    SLICK_CHECK_ERRNO(ret != SIG_ERR, "disableBoostTestSignalHandler.signal");
}


} // slick
