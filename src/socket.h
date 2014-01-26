/* socket.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 18 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Socket abstraction
*/

#pragma once

#include "address.h"

#include <algorithm>

namespace slick {


/******************************************************************************/
/* GUARD                                                                      */
/******************************************************************************/

struct FdGuard
{
    FdGuard(int fd) : fd(fd) {}
    ~FdGuard() { if (fd >= 0) close(fd); }

    int get() const { return fd; }

    int release()
    {
        int old = fd;
        fd = -1;
        return old;
    }

private:
    int fd;
};


/******************************************************************************/
/* SOCKET                                                                     */
/******************************************************************************/

struct Socket
{
    Socket() : fd_(-1) {}
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    int fd() const { return fd_; }
    int error() const;
    void throwError() const;

    operator bool() const { return fd_ >= 0; }

    static Socket connect(const Address& addr);
    static Socket connect(const NodeAddress& node);
    static Socket accept(int passiveFd);

private:
    void init();

    int fd_;
};


/******************************************************************************/
/* PASSIVE SOCKETS                                                            */
/******************************************************************************/

struct PassiveSockets
{
    PassiveSockets() {}
    explicit PassiveSockets(Port port);
    ~PassiveSockets();

    PassiveSockets(const PassiveSockets&) = delete;
    PassiveSockets& operator=(const PassiveSockets&) = delete;

    PassiveSockets(PassiveSockets&&) = default;
    PassiveSockets& operator=(PassiveSockets&&) = default;

    operator bool() const { return !fds_.empty(); }

    const std::vector<int>& fds() { return fds_; }
    bool test(int fd)
    {
        return std::find(fds_.begin(), fds_.end(), fd) != fds_.end();
    };

private:
    std::vector<int> fds_;
};


} // slick
