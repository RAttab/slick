/* socket.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 18 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Socket abstraction implementation
*/

#include "socket.h"
#include "utils.h"
#include "lockless/tls.h"

#include <array>
#include <string>
#include <cstring>
#include <cassert>
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

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
/* INTERFACE IT                                                               */
/******************************************************************************/

struct InterfaceIt
{
    InterfaceIt(const char* host, Port port) :
        first(nullptr), cur(nullptr)
    {
        struct addrinfo hints;
        std::memset(&hints, 0, sizeof hints);

        hints.ai_flags = !host ? AI_PASSIVE : 0;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        assert(!host || port);
        std::string portStr = std::to_string(port);

        int ret = getaddrinfo(host, portStr.c_str(), &hints, &first);
        if (ret) {
            SLICK_CHECK_ERRNO(ret != EAI_SYSTEM, "InterfaceIt.getaddrinfo");
            throw std::logic_error("error: " + std::to_string(ret));
        }

        cur = first;
    }

    ~InterfaceIt()
    {
        if (first) freeaddrinfo(first);
    }

    explicit operator bool() const { return cur; }
    void operator++ () { cur = cur->ai_next; }
    void operator++ (int) { cur = cur->ai_next; }

    const struct addrinfo& operator* () const { return *cur; }
    const struct addrinfo* operator-> () const { return cur; }

private:
    struct addrinfo* first;
    struct addrinfo* cur;
};


/******************************************************************************/
/* SOCKET                                                                     */
/******************************************************************************/


Socket::
Socket(Socket&& other) noexcept : fd_(other.fd_)
{
    other.fd_ = -1;
}


Socket&
Socket::
operator=(Socket&& other) noexcept
{
    if (this == &other) return *this;

    fd_ = other.fd_;
    other.fd_ = -1;

    return *this;
}


Socket
Socket::
connect(const Address& addr)
{
    Socket socket;
    assert(addr);

    for (InterfaceIt it(addr.chost(), addr.port); it; it++) {

        int flags = SOCK_NONBLOCK;
        int fd = ::socket(it->ai_family, it->ai_socktype | flags, it->ai_protocol);
        if (fd < 0) continue;

        FdGuard guard(fd);

        int ret = ::connect(fd, it->ai_addr, it->ai_addrlen);
        if (ret < 0 && errno != EINPROGRESS) continue;

        socket.fd_ = guard.release();
        break;
    }

    if (socket) socket.init();
    return std::move(socket);
}

Socket
Socket::
connect(const std::vector<Address>& addrs)
{
    for (const auto& addr : addrs) {
        Socket socket = connect(addr);
        if (socket) return std::move(socket);
    }
    return Socket();
}


Socket
Socket::
accept(int fd)
{
    Socket socket;

    socklen_t addrlen = 0;
    struct sockaddr addr;
    std::memset(&addr, 0, sizeof(addr));

    socket.fd_ = accept4(fd, &addr, &addrlen, SOCK_NONBLOCK);
    if (socket.fd_ < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return std::move(socket);
    SLICK_CHECK_ERRNO(socket.fd_ >= 0, "Socket.accept");

    socket.init();
    return std::move(socket);
}

void
Socket::
init()
{
    int val = true;
    int ret = setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &val, sizeof val);
    SLICK_CHECK_ERRNO(!ret, "Socket.setsockopt.TCP_NODELAY");
}

Socket::
~Socket()
{
    if (fd_ < 0) return;

    // There's no error checking because there's not much we can do if they fail
    shutdown(fd_, SHUT_RDWR);
    close(fd_);
}

int
Socket::
error() const
{
    int error = 0;
    socklen_t errlen = sizeof error;

    int ret = getsockopt(fd_, SOL_SOCKET, SO_ERROR, &error, &errlen);
    SLICK_CHECK_ERRNO(!ret, "Socket.getsockopt.error");

    return error;
}

void
Socket::
throwError() const
{
    int err = error();
    if (err) throw std::runtime_error(checkErrnoString(err, "Socket.error"));
}


/******************************************************************************/
/* PASSIVE SOCKET                                                             */
/******************************************************************************/

PassiveSockets::
PassiveSockets(Port port)
{
    for (InterfaceIt it(nullptr, port); it; it++) {

        int flags = SOCK_NONBLOCK;
        int fd = socket(it->ai_family, it->ai_socktype | flags, it->ai_protocol);
        if (fd < 0) continue;

        FdGuard guard(fd);

        int ret = bind(fd, it->ai_addr, it->ai_addrlen);
        if (ret < 0) continue;

        ret = listen(fd, 1U << 8);
        if (ret < 0) continue;

        fds_.push_back(guard.release());
    }

    if (fds_.empty()) throw std::runtime_error("ERROR: no valid interface");
}

PassiveSockets::
~PassiveSockets()
{
    for (int fd : fds_) close(fd);
}


/******************************************************************************/
/* NETWORK INTERFACES                                                         */
/******************************************************************************/

std::vector<Address>
networkInterfaces(bool excludeLoopback)
{
    std::vector<Address> result;
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

