/* provider.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Implementation of the provider endpoint
*/

#include "provider.h"

#include <sys/socket.h>

/******************************************************************************/
/* ENDPOINT PROVIDER                                                          */
/******************************************************************************/

namespace slick {

EndpointProvider::
EndpointProvider(PortRange ports) :
    sockets(ports, SOCK_NONBLOCK)
{
    for (int fd : sockets.fds())
        poller.add(fd, EPOLLET | EPOLLIN);
}


EndpointProvider::
~EndpointProvider()
{
    for (int fd : sockets.fds())
        poller.del(fd);
}


void
EndpointProvider::
publish(std::shared_ptr<Naming> name, const std::string&)
{
    this->name = std::move(name);
    // name->publish(endpoint, ...);
}

void
EndpointProvider::
onPollEvent(struct epoll_event& ev)
{
    assert(ev.events == EPOLLIN);
    assert(sockets.test(ev.data.fd));

    while (true) {
        Socket socket = Socket::accept(ev.data.fd, SOCK_NONBLOCK);
        if (socket.fd() < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;

        connect(std::move(socket));
    }
}

} // slick
