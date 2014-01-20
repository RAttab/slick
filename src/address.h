/* address.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 19 Jan 2014
   FreeBSD-style copyright and disclaimer apply

   Network address utilities.
*/

#pragma once

#include "pack.h"

#include <vector>
#include <string>
#include <cstdint>
#include <sys/socket.h>

namespace slick {


/******************************************************************************/
/* PORT                                                                       */
/******************************************************************************/

typedef uint16_t Port;


/******************************************************************************/
/* PORT RANGE                                                                 */
/******************************************************************************/

struct PortRange
{
    Port first, last;

    PortRange(Port port) : first(port), last(port + 1) {}
    PortRange(Port first, Port last) : first(first), last(last) {}

    size_t size() const { return last - first; }
    bool includes(Port port) const
    {
        return port >= first && port < last;
    }

};


/******************************************************************************/
/* ADDRESS                                                                    */
/******************************************************************************/

struct Address
{
    Address() : port(0) {}
    Address(struct sockaddr* addr);
    Address(struct sockaddr* addr, socklen_t addrlen);
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


// \todo Should pack the host as binary instead of a string.
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
/* NETWORK INTERFACES                                                         */
/******************************************************************************/

std::vector<Address> networkInterfaces(bool excludeLoopback = false);

} // slick
