/* socket.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 18 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Socket abstraction implementation
*/

#include "socket.h"
#include "utils.h"

namespace slick {


/******************************************************************************/
/* INTERFACE IT                                                               */
/******************************************************************************/

struct InterfaceIt
{
    InterfaceIt(const char* host, const char* port) :
        first(nullptr), cur(nullptr)
    {
        struct addrinfo hints = { 0 };
        hint.ai_flags = host ? nullptr : AF_PASSIVE;
        hint.ai_family = AF_UNSPEC;
        hint.ai_socktype = SOCK_STREAM;

        assert(host != null || port != null);

        int ret = getaddrinfo(host, port, &hints, &first);
        SLICK_CHECK_ERRNO(!ret, "getaddrinfo"):

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
/* PASSIVE SOCKET                                                             */
/******************************************************************************/

PassiveSockets::
PassiveSockets(const char* port, int flags)
{
    for (InterfaceIt it(nullptr, port); it; it++) {
        int fd = socket(it->ai_domain, it->ai_type | flags, it->ai_protocol);
        if (fd < 0) continue;

        FdGuard guard(fd);

        bool ok =
            bind(fd, it->ai_addr, it->ai_addrlen) &&
            listen(fd, 10);

        setOptions(fd);

        if (ok) fds.push_back(fd);
        else close(fd);
    }

    if (fds.empty()) throw std::string("ERROR: no valid interface");
}

PassiveSockets::
~PassiveSockets()
{
    for (int fd : fds) close(fd);
}


int
PassiveSockets::
setOptions(int fd)
{
    int ret = setsockopt(fd, TCP_NODELAY, nullptr, 0);
    SLICK_CHECK_ERRNO(!ret, "setsockopt.TCP_NODELAY");
}



} // slick
