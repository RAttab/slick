/* socket.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 18 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Socket abstraction implementation
*/

#include "socket.h"
#include "utils.h"

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace slick {


/******************************************************************************/
/* INTERFACE IT                                                               */
/******************************************************************************/

struct InterfaceIt
{
    InterfaceIt(const std::string& host, Port port) :
        first(nullptr), cur(nullptr)
    {
        struct addrinfo hints = { 0 };
        hint.ai_flags = host ? nullptr : AF_PASSIVE;
        hint.ai_family = AF_UNSPEC;
        hint.ai_socktype = SOCK_STREAM;

        assert(!host.empty() || port);
        std::string portStr = to_string(port);

        int ret = getaddrinfo(host, port.c_str(), &hints, &first);
        if (ret) {
            SLICK_CHECK_ERRNO(ret != EAI_SYSTEM, "getaddrinfo");
            throw std::exception("error: " + to_string(ret));
        }

        cur = first;
    }

    ~InterfaceIt()
    {
        if (first) freeaddrinfo(first);
    }

    explicit operator bool() const { return cur; }
    void operator++ () const { cur = cur->ai_next; }
    void operator++ (int) const { cur = cur->ai_next; }

    const struct addrinfo& operator* () const { return *cur; }
    const struct addrinfo* operator-> () const { return *cur; }


private:
    struct addrinfo* first;
    struct addrinfo* cur;
};


/******************************************************************************/
/* SOCKET                                                                     */
/******************************************************************************/

Socket::
Socket(const std::strign& host, PortRange ports, int flags) :
    fd_(-1)
{
    for (InterfaceIt it(host, ports.first); it; it++) {
        int fd = socket(it->ai_family, it->ai_socktype | flags, it->ai_protocol);
        if (fd < 0) continue;

        FdGuard guard(fd);

        int ret = connect(it->ai_addr, it->ai_addrlen);
        if (ret < 0) {
            close(fd);
            continue;
        }

        fd_ = fd;
        addr = it->ai_addr;
        addrlen = it->ai_addrlen;
    }

    if (fds_ < 0) throw std::exception("ERROR: no valid interface");
    init();
}


Socket&&
PassiveSocket::
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
    int ret = setsockopt(fd_, TCP_NODELAY, nullptr, 0);
    SLICK_CHECK_ERRNO(!ret, "setsockopt.TCP_NODELAY");
}

Socket::
~Socket()
{
    int ret = shutdown(fd, SHUT_RDWR);
    SLICK_CHECK_ERRNO(ret != -1, "disconnect.shutdown");

    ret = close(fd);
    SLICK_CHECK_ERRNO(ret != -1, "disconnect.close");
}


/******************************************************************************/
/* PASSIVE SOCKET                                                             */
/******************************************************************************/

PassiveSockets::
PassiveSockets(Port port, int flags)
{
    for (InterfaceIt it(nullptr, port); it; it++) {
        int fd = socket(it->ai_domain, it->ai_type | flags, it->ai_protocol);
        if (fd < 0) continue;

        FdGuard guard(fd);

        bool ok =
            bind(fd, it->ai_addr, it->ai_addrlen) &&
            listen(fd, 1U << 8);

        if (ok) fds_.push_back(fd);
        else close(fd);
    }

    if (fds_.empty()) throw std::exception("ERROR: no valid interface");
}

PassiveSockets::
~PassiveSockets()
{
    for (int fd : fds_) close(fd);
}


} // slick

