/* socket.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 18 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Socket abstraction
*/

#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>
#include <unistd.h>
#include <sys/socket.h>

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
/* PORT RANGE                                                                 */
/******************************************************************************/

typedef uint16_t Port;

struct PortRange
{
    PortRange(Port port) : first(port), last(port + 1) {}
    PortRange(Port first, Port last) : first(first), last(last) {}

    Port first, last;
};


/******************************************************************************/
/* SOCKET                                                                     */
/******************************************************************************/

struct Socket
{
    Socket() : fd_(-1) {}
    Socket(const std::string& host, PortRange ports, int flags = 0);
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    int fd() const { return fd_; }

    static Socket&& accept(int passiveFd, int flags = 0);

private:
    void init();

    int fd_;
    struct sockaddr addr;
    socklen_t addrlen;
};


/******************************************************************************/
/* PASSIVE SOCKETS                                                            */
/******************************************************************************/

struct PassiveSockets
{
    explicit PassiveSockets(Port port, int flags = 0);
    ~PassiveSockets();

    PassiveSockets(const PassiveSockets&) = delete;
    PassiveSockets& operator=(const PassiveSockets&) = delete;

    const std::vector<int>& fds() { return fds_; }
    bool test(int fd)
    {
        return std::find(fds_.begin(), fds_.end(), fd) != fds_.end();
    };

private:
    std::vector<int> fds_;
};

} // slick
