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

namespace slick {


/******************************************************************************/
/* ENDPOINT PROVIDER                                                          */
/******************************************************************************/

EndpointProvider(std::string name, const char* port) :
    name(std::move(name)), sockets(port)
{
    pollFd = epoll_create(1);
    SLICK_CHECK_ERRNO(pollFd != -1, "epoll_create");

    struct epoll_event ev = { 0 };
    ev.events = EPOLLIN | EPOLLET;

    for (int fd : sockets.fds()) {
        ev.events.fd = fd;

        int ret = epoll_ctl(pollfd, EPOLL_CTL_ADD, fd, &ev);
        SLICK_CHECK_ERRNO(ret != -1, "epoll_create");
    }
}

void
EndpointProvider::
~EndpointProvider()
{
    close(pollFd);
}


void
EndpointProvider::
publish(const std::string& endpoint)
{
    // \todo register ourself with zk here.
}

void
EndpointProvider::
send(const ClientHandle& client, Message&& msg)
{

}

void
EndpointProvider::
broadcast(Message&& msg)
{

}

void
EndpointProvider::
listen()
{
    while(true)
    {

    }
}

void
EndpointProvider::
connectClient()
{

}

void
EndpointProvider::
disconnectClient()
{

}

void
EndpointProvider::
processMessage(int fd)
{

}

} // slick
