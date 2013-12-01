/* notify.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 30 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   fd notification.
*/

#pragma once

namespace slick {

/******************************************************************************/
/* NOTIFY                                                                     */
/******************************************************************************/

struct Notify
{
    Notify();
    ~Notify();

    Notify(const Notify&) = delete;
    Notify& operator=(const Notify&) = delete;

    int fd() const { return fd_; }

    bool poll();
    void signal();

private:
    int fd_;
};

} // slick
