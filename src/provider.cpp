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
    name(std::move(name)), sockets(port, O_NONBLOCK), pollThread(0)
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
send(const ClientHandle& h, Message&& msg)
{
    std::assert(threadId() == pollThread);

    auto it = clients.find(h);
    std::assert(it != clients.end());

    if (!it->send(toChunkedHttp(msg)))
        disconnect(it->fd);
}

void
EndpointProvider::
broadcast(Message&& msg)
{
    std::assert(threadId() == pollThread);

    Message httpMsg = toChunkedHttp(msg);

    for (auto& client : clients) {
        if(!client.send(httpMsg))
            disconnect(client.fd);
    }
}

void
EndpointProvider::
poll()
{
    pollThread = threadId();

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

            else if (ev.events == EPOLLOUT)
                sendMessage(ev.data.fd);

            else if (ev.events == EPOLLIN)
                recvMessage(ev.data.fd);
        }

        double now = wall();
        if (nextHeartbeat < now) {
            sendHeartbeat();
            nextHeartbeat = now + heartbeatFreq;
        }
    }

    pollThread = 0;
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

        struct epoll_event ev = { 0 };
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.events(pollFd, fd, &ev);

        int ret = epoll_ctl(pollfd, EPOLL_CTL_ADD, fd, &ev);
        SLICK_CHECK_ERRNO(ret != -1, "epoll_ctl.client");

        fd.release();

        clients[fd] = std::move(client);
        sendHeader(fd);
    }
}

void
EndpointProvider::
disconnectClient()
{

}

void
EndpointProvider::
recvMessage(int fd)
{
    auto it = clients.find(fd);
    std::assert(it != clients.end());


}

void
EndpointProvider::
sendMessage(int fd)
{
    auto it = clients.find(fd);
    std::assert(it != clients.end());
    it->flushQueue();
}


void
EndpointProvider::
sendHeartbeats()
{
    static const char msg[] = "HEARTBEAT";
    static const size_t length = sizeof msg;

}

void
EndpointProvider::
sendHeader(int fd)
{
    static const char msg[] = "";
    static const size_t length = sizeof msg;

    auto it = clients.find(fd);
    std::assert(it != clients.end());
    it->send(Message(msg, length));
}


/******************************************************************************/
/* CLIENT STATE                                                               */
/******************************************************************************/

namespace {

template<typename Msg>
bool send(EndpointProvider::ClientState& state, Msg&& msg)
{
    if (!state.writable) {
        state.sendQueue.emplace_back(std::forward(msg));
        return;
    }

    ssize_t sent = send(state.fd, msg.bytes(), msg.size(), MSG_NOSIGNAL);
    if (sent >= 0) {
        std::assert(sent == msg.size());
        return;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        state.writable = false;
        state.sendQueue.emplace_back(std::forward(msg));
        return;
    }

    else if (errno == ECONNRESET || errno == EPIPE) return false;

    SLICK_CHECK_ERRNO(sent >= 0, "send.header");

    state.bytesSent += sent;
    return true;
}

} // namespace anonymous

bool
EndpointProvider::ClientState::
send(Message&& msg)
{
    return send(*this, std::move(msg));
}

bool
EndpointProvider::ClientState::
send(const Message& msg)
{
    return send(*this, msg);
}

void
EndpointProvider::ClientState::
flushQueue()
{
    writable = true;
    std::vector<Message> queue = std::move(sendQueue);
    for (msg& : queue) send(std::move(msg));
}


} // slick
