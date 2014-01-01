/* socket.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 18 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Socket abstraction
*/

#pragma once

#include "pack.h"

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
/* ADDRESS                                                                    */
/******************************************************************************/

struct Address
{
    Address() : port(0) {}
    Address(const std::string& hostPort);
    Address(std::string host, Port port) :
        host(std::move(host)), port(port)
    {}

    operator bool() const { return host.size() && port; }

    bool operator< (const Address& other)
    {
        int res = host.compare(other.host);
        if (res) return res < 0;

        return port < other.port;
    }

    const char* chost() const { return host.c_str(); }

    std::string toString() const
    {
        return host + ':' + std::to_string(port);
    }

    std::string host;
    Port port;
};


template<>
struct Pack<Address>
{
    static size_t size(const Address& value)
    {
        return packedSizeAll(value.host, value.port);
    }

    static void pack(const Address& value, PackIt first, PackIt last)
    {
        packAll(first, last, value.host, value.port);
    }

    static Address unpack(ConstPackIt first, ConstPackIt last)
    {
        Address value;
        unpackAll(first, last, value.host, value.port);
        return std::move(value);
    }
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

    static Socket connect(const Address& addr, int flags = 0);
    static Socket accept(int passiveFd, int flags = 0);

private:
    void init();

    int fd_;
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

    PassiveSockets(PassiveSockets&&) = default;
    PassiveSockets& operator=(PassiveSockets&&) = default;


    const std::vector<int>& fds() { return fds_; }
    bool test(int fd)
    {
        return std::find(fds_.begin(), fds_.end(), fd) != fds_.end();
    };

    std::vector<Address> interfaces() const;

private:
    std::vector<int> fds_;
};

} // slick
