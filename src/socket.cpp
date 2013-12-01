/* socket.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 18 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Socket abstraction implementation
*/

#include "socket.h"
#include "utils.h"

#include <string>
#include <cstring>
#include <cassert>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

namespace slick {


/******************************************************************************/
/* INTERFACE IT                                                               */
/******************************************************************************/

struct InterfaceIt
{
    InterfaceIt(const std::string& host, Port port) :
        first(nullptr), cur(nullptr)
    {
        struct addrinfo hints;
        std::memset(&hints, 0, sizeof hints);

        hints.ai_flags = host.empty() ? AI_PASSIVE : 0;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        assert(!host.empty() || port);
        std::string portStr = std::to_string(port);

        int ret = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &first);
        if (ret) {
            SLICK_CHECK_ERRNO(ret != EAI_SYSTEM, "getaddrinfo");
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
Socket(const std::string& host, PortRange ports, int flags) :
    fd_(-1)
{
    for (InterfaceIt it(host, ports.first); it; it++) {
        int fd = socket(it->ai_family, it->ai_socktype | flags, it->ai_protocol);
        if (fd < 0) continue;

        FdGuard guard(fd);

        int ret = connect(fd, it->ai_addr, it->ai_addrlen);
        if (ret < 0) {
            close(fd);
            continue;
        }

        fd_ = fd;
        addrlen = it->ai_addrlen;
        std::memcpy(&addr, &it->ai_addr, sizeof addr);
    }

    if (fd_ < 0) throw std::runtime_error("ERROR: no valid interface");
    init();
}


Socket&&
Socket::
accept(int fd, int flags)
{
    Socket socket;

    socket.fd_ = accept4(fd, &socket.addr, &socket.addrlen, flags);
    if (socket.fd_ < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return std::move(socket);
    SLICK_CHECK_ERRNO(socket.fd_ >= 0, "accept");

    socket.init();
    return std::move(socket);
}

void
Socket::
init()
{
    int ret = setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, nullptr, 0);
    SLICK_CHECK_ERRNO(!ret, "setsockopt.TCP_NODELAY");
}

Socket::
~Socket()
{
    int ret = shutdown(fd_, SHUT_RDWR);
    SLICK_CHECK_ERRNO(ret != -1, "disconnect.shutdown");

    ret = close(fd_);
    SLICK_CHECK_ERRNO(ret != -1, "disconnect.close");
}


/******************************************************************************/
/* PASSIVE SOCKET                                                             */
/******************************************************************************/

PassiveSockets::
PassiveSockets(Port port, int flags)
{
    for (InterfaceIt it(nullptr, port); it; it++) {
        int fd = socket(it->ai_family, it->ai_socktype | flags, it->ai_protocol);
        if (fd < 0) continue;

        FdGuard guard(fd);

        bool ok =
            bind(fd, it->ai_addr, it->ai_addrlen) &&
            listen(fd, 1U << 8);

        if (ok) fds_.push_back(fd);
        else close(fd);
    }

    if (fds_.empty()) throw std::runtime_error("ERROR: no valid interface");
}

PassiveSockets::
~PassiveSockets()
{
    for (int fd : fds_) close(fd);
}


} // slick

