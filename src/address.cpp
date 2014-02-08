/* address.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 19 Jan 2014
   FreeBSD-style copyright and disclaimer apply

   Implemenation of the various network utilities.
*/

#include "address.h"
#include "utils.h"

#include <array>
#include <cassert>
#include <cstdlib>
#include <unistd.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace slick {


/******************************************************************************/
/* ADDRESS                                                                    */
/******************************************************************************/

Address::
Address(const std::string& hostPort)
{
    size_t pos = hostPort.find(':');
    assert(pos != std::string::npos);

    host = hostPort.substr(0, pos);
    port = stoi(hostPort.substr(pos + 1));
}

Address::
Address(struct sockaddr* addr)
{
    socklen_t addrlen;
    int family = addr->sa_family;

    if (family == AF_INET) addrlen = sizeof(struct sockaddr_in);
    else if (family == AF_INET6) addrlen = sizeof(struct sockaddr_in6);
    else assert(false);

    *this = Address(addr, addrlen);
}

Address::
Address(struct sockaddr* addr, socklen_t addrlen)
{
    std::array<char, 256> host;
    std::array<char, 256> service;

    int res = getnameinfo(
            addr, addrlen,
            host.data(), host.size(),
            service.data(), service.size(),
            NI_NUMERICHOST | NI_NUMERICSERV);
    SLICK_CHECK_ERRNO(!res, "Address.getnameinfo");

    this->host = std::string(host.data());
    this->port = atoi(service.data());
}


/******************************************************************************/
/* NODE ADDRESS                                                               */
/******************************************************************************/

NodeAddress
addrToNode(Address addr)
{
    return { std::move(addr) };
}

std::vector<NodeAddress>
addrToNode(std::vector<Address> addrs)
{
    std::vector<NodeAddress> result;
    result.reserve(addrs.size());

    for (auto& addr : addrs)
        result.emplace_back(addrToNode(std::move(addr)));

    return result;
}


/******************************************************************************/
/* NETWORK INTERFACES                                                         */
/******************************************************************************/

NodeAddress
networkInterfaces(bool excludeLoopback)
{
    NodeAddress result;
    unsigned include = IFF_UP | IFF_RUNNING;
    unsigned exclude = excludeLoopback ? IFF_LOOPBACK : 0;

    struct ifaddrs* it;
    int ret = getifaddrs(&it);
    SLICK_CHECK_ERRNO(!ret, "networkInterfaces.getifaddrs");
    auto addrsGuard = guard([=] { freeifaddrs(it); });

    for(; it; it = it->ifa_next) {

        if (!it->ifa_addr) continue;
        if (it->ifa_flags & exclude) continue;
        if ((it->ifa_flags & include) != include) continue;

        int family = it->ifa_addr->sa_family;
        if (family != AF_INET && family != AF_INET6) continue;

        result.emplace_back(it->ifa_addr);
    }

    return std::move(result);
}

} // slick
