/* provider.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Implementation of the provider endpoint
*/

#include <functional>

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "provider.h"
#include "utils.h"


/******************************************************************************/
/* ENDPOINT PROVIDER                                                          */
/******************************************************************************/

namespace slick {

EndpointProvider(const char* port) :
    sockets(port, O_NONBLOCK)
{
    for (int fd : sockets.fds())
        poller.add(fd, EPOLLET | EPOLLIN);
}


void
EndpointProvider::
~EndpointProvider()
{
    for (int fd : sockets.fds())
        poller.del(fd);
}


void
EndpointProvider::
publish(const std::string& endpoint)
{
    // \todo register ourself with zk here.
}

void
EndpointProvider::
onPollEvent(struct epoll_event& ev)
{
    std::assert(ev.events == EPOLLIN);
    std::assert(sockets.test(ev.data.fd));

    while (true) {
        ActiveSocket socket = ActiveSocket::accept(ev.data.fd, O_NONBLOCK);
        if (socket.fd() < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;

        connect(std::move(socket));
    }
}

} // slick
