/* notify.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 30 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Notify implementation
*/

#include "notify.h"

namespace slick {


/******************************************************************************/
/* NOTIFY                                                                     */
/******************************************************************************/

Notify::
Notify()
{
    fd_ = eventfd(0, EFD_NONBLOCK);
    SLICK_CHECK_ERRNO(fd >= 0, "notify.eventfd");
}

Notify::
~Notify() { close(fd_); }

bool
Notify::
poll()
{
    eventfd_t val;
    int ret = read(fd_, &val, sizeof val);

    if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return false;
    SLICK_CHECK_ERRNO(!ret, "notify.read");

    return true;
}

void
Notify::
signal()
{
    eventfd_t val = 1;
    int ret = eventfd_write(fd_, val);
    SLICK_CHECK_ERRNO(!ret, "notify.write");
}

} // slick
