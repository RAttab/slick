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
    // The extra step is required to not invalidate our iterator
    std::vector<int> toDisconnect;
    for (const auto& client : clients)
        toDisconnect.push_back(client.first);

    for (int fd : toDisconnect)
        disconnectClient(fd);

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
poll()
{
    pollThread = threadId();

    enum { MaxEvents = 10 };
    struct epoll_event events[MaxEvents];

    while(true)
    {
        int n = epoll_wait(pollFd, events, MaxEvents, 100);
        if (n < 0 && errno == EINTR) continue;
        SLICK_CHECK_ERRNO(n >= 0, "epoll_wait");

        for (size_t i = 0; i < n; ++i) {
            const auto& ev = events[i];
            SLICK_CHECK_ERRNO(!(ev.events & EPOLLERR), "epoll_wait.EPOLLERR");

            if (sockets.test(ev.data.fd)) {
                std::assert(ev.events == EPOLLIN);
                connectClient(ev.data.fd);
                continue;
            }

            if (ev.events & EPOLLIN) recvMessage(ev.data.fd);

            if (ev.events & EPOLLRDHUP || ev.events & EPOLLHUP) {
                disconnectClient(ev.data.fd);
                continue;
            }

            if (ev.events & EPOLLOUT) flushQueue(ev.data.fd);
        }
    }

    pollThread = 0;
}

void
EndpointProvider::
connectClient(int fd)
{
    while (true) {
        ActiveSocket socket = ActiveSocket::accept(fd, O_NONBLOCK);
        if (socket.fd() < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;

        struct epoll_event ev = { 0 };
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.events(pollFd, fd, &ev);

        ret = epoll_ctl(pollfd, EPOLL_CTL_ADD, fd, &ev);
        SLICK_CHECK_ERRNO(ret != -1, "epoll_ctl.client");

        ClientState client;
        client.socket = std::move(socket);
        clients[client.socket.fd()] = std::move(client);
    }
}


void
EndpointProvider::
disconnectClient(int fd)
{
    clients.erase(fd);
    onLostClient(fd);
}


void
EndpointProvider::
recvMessage(int fd)
{
    auto it = clients.find(fd);
    std::assert(it != clients.end());

    enum { bufferLength = 1U << 16 };
    uint8_t buffer[bufferLength];

    while (true) {
        ssize_t read = recv(fd, buffer, bufferLength, 0);
        if (read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;
            SLICK_CHECK_ERRNO(read != -1, "recv");
        }

        std::assert(read < bufferLength);
        if (!read) { // indicates that shutdown was called on the client side.
            disconnectClient(fd);
            break;
        }

        it->bytesRecv += read;
        onMessage(fd, Message(buffer, read));
    }

}


namespace {

// This is not part of the class EndpointProvider because we don't want to make
// it part of the header.

template<typename Msg>
bool sendToClient(EndpointProvider::ClientState& client, Msg&& msg)
{
    if (!client.writable) {
        client.sendQueue.emplace_back(std::forward(msg));
        return;
    }

    ssize_t sent =
        send(client.socket.fd(), msg.bytes(), msg.size(), MSG_NOSIGNAL);
    if (sent >= 0) {
        std::assert(sent == msg.size());
        return true;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        client.writable = false;
        client.sendQueue.emplace_back(std::forward(msg));
        return true;
    }

    else if (errno == ECONNRESET || errno == EPIPE) return false;

    SLICK_CHECK_ERRNO(sent >= 0, "send.header");

    client.bytesSent += sent;
    return true;
}

} // namespace anonymous

void
EndpointProvider::
send(const ClientHandle& h, Message&& msg)
{
    std::assert(threadId() == pollThread);

    auto it = clients.find(h);
    std::assert(it != clients.end());

    if (!sendToClient(it->second, std::move(msg)))
        disconnectClient(it->first);
}

void
EndpointProvider::
broadcast(Message&& msg)
{
    std::assert(threadId() == pollThread);

    std::vector<int> toDisconnect;

    for (auto& client : clients) {
        if (!sendToClient(client, msg))
            toDisconnect.push_back(client.first);
    }

    for (int fd : toDisconnect) disconnectClient(fd);
}

void
EndpointProvider::
flushQueue(int fd)
{
    auto it = clients.find(fd);
    std::assert(it != clients.end());

    ClientState& client = it->second;
    client.writable = true;

    std::vector<Message> queue = std::move(client.sendQueue);
    for (msg& : queue) {
        if (sendToClient(client, std::move(msg))) continue;

        disconnectClient(fd);
        break;
    }
}

} // slick
