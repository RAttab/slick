/* utils.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 18 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   random utiltiies
*/

#pragma once

#include <string>
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


} // namespace slick
