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
/* MESSAGES                                                                   */
/******************************************************************************/

namespace {

namespace Msg {

const char rawHearbeat[] = "__HB__";
const Message heartbeat = makeHttpMessage(rawHeartbeat, sizeof rawHeartbeat);


#undef makeHttpMessage

} // namespace Msg

} // namespace anonymous

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
send(const ClientHandle& h, Message&& msg)
{
    std::assert(threadId() == pollThread);

    auto it = clients.find(h);
    std::assert(it != clients.end());

    if (!it->send(std::move(msg)))
        disconnect(it->fd);
}

void
EndpointProvider::
broadcast(Message&& msg)
{
    std::assert(threadId() == pollThread);

    for (auto& client : clients) {
        if(!client.send(msg))
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

    double heartbeatFreq = HeartbeatFrequencyMs / 1000.0
    double nextHearbeat = wall() + heartbeatFreq;

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

            if (ev.events & EPOLLOUT) sendMessage(ev.data.fd);
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

        int ret = setsockopt(fd, TCP_NODELAY, nullptr, 0);
        SLICK_CHECK_ERRNO(!ret, "setsockopt.TCP_NODELAY");

        struct epoll_event ev = { 0 };
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.events(pollFd, fd, &ev);

        int ret = epoll_ctl(pollfd, EPOLL_CTL_ADD, fd, &ev);
        SLICK_CHECK_ERRNO(ret != -1, "epoll_ctl.client");

        fd.release();

        clients[fd] = std::move(client);
    }
}


void
EndpointProvider::
disconnectClient(int fd)
{
    int ret = shutdown(fd, SHUT_RDWR);
    SLICK_CHECK_ERRNO(ret != -1, "shutdown");

    ret = close(fd);
    SLICK_CHECK_ERRNO(ret != -1, "disconnect.close");

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
        if (!read) {
            disconnectClient(fd);
            break;
        }

        it->bytesRecv += read;

        Message msg(buffer, read);
        if (msg == Msg::heartbeat) it->lastHeartbeatRecv = wall();
        else onMessage(fd, std::move(msg));
    }

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
    double now = wall();

    std::vector<int> toDisconnect;
    for (auto& entry : clients) {
        auto& client = entry.second;

        if (now - client.lastHearbeatRecv < HeartbeatThresholdMs / 1000.0) {
            client.send(Msg::heartbeat);
            client.lastHeartbeatSent = now;
        }
        else toDisconnect.push_back(entry.first);
    }

    for (int fd : toDisconnect) disconnect(fd);
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
