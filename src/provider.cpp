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
    name(std::move(name)), sockets(port, O_NONBLOCK)
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
poll()
{
    enum { MaxEvents = 10 };
    struct epoll_event events[MaxEvents];

    static const double heartbeatFreq = 0.1;
    double nextHearbeat = wall() + heartbeatFreq;

    while(true)
    {
        int n = epoll_wait(pollFd, events, MaxEvents, 100);
        if (n < 0 && errno == EINTR) continue;
        SLICK_CHECK_ERRNO(n >= 0, "epoll_wait");

        for (size_t i = 0; i < n; ++i) {
            const auto& ev = events[i];
            SLICK_CHECK_ERRNO(ev.events != EPOLLERR, "epoll_wait.EPOLLERR");
            SLICK_CHECK_ERRNO(ev.events != EPOLLHUP, "epoll_wait.EPOLLHUP");

            if (sockets.test(ev.data.fd))
                connectClient(ev.data.fd);
            else processMessage(ev.data.fd);
        }

        double now = wall();
        if (nextHeartbeat < now) {
            sendHeartbeat();
            nextHeartbeat = now + heartbeatFreq;
        }
    }
}

void
EndpointProvider::
connectClient(int fd)
{
    while (true) {
        ClientState client;

        int fd = accept4(fd, &client.addr, &client.addrlen, O_NONBLOCK);
        if (fd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        SLICK_CHECK_ERRNO(fd >= 0, "accept");

        FdGuard(fd);

        sendHeader(fd);

        struct epoll_event ev = { 0 };
        ev.events = EPOLLIN | EPOLLET;
        ev.events(pollFd, fd, &ev);

        int ret = epoll_ctl(pollfd, EPOLL_CTL_ADD, fd, &ev);
        SLICK_CHECK_ERRNO(ret != -1, "epoll_ctl.client");

        fd.release();

        clients[fd] = std::move(client);
    }
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
    auto it = clients.find(fd);
    assert(it != clients.end());


}


void
EndpointProvider::
sendHeartbeats()
{
    Message heartbeat;

}

void
EndpointProvider::
sendHeader(int fd)
{

}


} // slick
