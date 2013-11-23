/* utils.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 23 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Utilties for slick network
*/

#include "utils.h"

#include <atomic>

namespace slick {

namespace {

std::atomic<size_t> nextThreadId(0);
slickTls myThreadId = 0;

} // namespace anonymous

size_t threadId()
{
    if (!myThreadId) myThreadId = ++nextThreadId;
    return myThreadId;
}


} // slick
