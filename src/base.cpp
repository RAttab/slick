/* base.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 29 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Endpoint base implementation.
*/

#include "base.h"
#include "utils.h"

#include <cassert>
#include <sys/epoll.h>

namespace slick {

/******************************************************************************/
/* ENDPOINT BASE                                                              */
/******************************************************************************/

EndpointBase::
EndpointBase() : pollThread(0)
{
    poller.add(messagesFd.fd());
}


void
EndpointBase::
~EndpointBase()
{
    // The extra step is required to not invalidate our iterator
    std::vector<int> toDisconnect;
    for (const auto& connection : connections)
        toDisconnect.push_back(connection.first);

    for (int fd : toDisconnect)
        disconnect(fd);
}


void
EndpointBase::
poll()
{
    pollThread = threadId();

    while(poller.poll()) {

        struct epoll_event ev = poller.next();
        SLICK_CHECK_ERRNO(!(ev.events & EPOLLERR), "epoll_wait.EPOLLERR");

        if (connections.count(ev.data.fd)) {
            if (ev.events & EPOLLIN) recvPayload(ev.data.fd);

            if (ev.events & EPOLLRDHUP || ev.events & EPOLLHUP) {
                disconnect(ev.data.fd);
                continue;
            }

            if (ev.events & EPOLLOUT) flushQueue(ev.data.fd);
        }

        else if (ev.data.fd == messagesFd.fd())
            flushMessages();

        else onPollEvent(ev);
    }
}


void
EndpointBase::
connect(Socket&& socket)
{
    poller.add(socket.fd(), EPOLLET | EPOLLIN | EPOLLOUT);

    ConnectionState connection;
    connection.socket = std::move(socket);
    connections[connection.socket.fd()] = std::move(connection);
}


void
EndpointBase::
disconnect(int fd)
{
    poller.del(fd);
    connections.erase(fd);
    onLostConnection(fd);
}


void
EndpointBase::
recvPayload(int fd)
{
    auto it = connections.find(fd);
    std::assert(it != connections.end());

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
        if (!read) { // indicates that shutdown was called on the connection side.
            disconnect(fd);
            break;
        }

        it->bytesRecv += read;
        onPayload(fd, Payload(buffer, read));
    }

}


namespace {

// This is not part of the class EndpointBase because we don't want to make
// it part of the header.

template<typename Payload>
bool sendTo(EndpointBase::ConnectionState& connection, Payload&& msg)
{
    if (!connection.writable) {
        connection.sendQueue.emplace_back(std::forward(msg));
        return;
    }

    ssize_t sent =
        send(connection.socket.fd(), msg.bytes(), msg.size(), MSG_NOSIGNAL);
    if (sent >= 0) {
        std::assert(sent == msg.size());
        return true;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        connection.writable = false;
        connection.sendQueue.emplace_back(std::forward(msg));
        return true;
    }

    else if (errno == ECONNRESET || errno == EPIPE) return false;

    SLICK_CHECK_ERRNO(sent >= 0, "send.header");

    connection.bytesSent += sent;
    return true;
}

} // namespace anonymous


void
EndpointBase::
send(int fd, Payload&& msg)
{
    if (threadId() != pollThread) {
        messages.push(Message(fd, std::move(msg)));
        messagesFd.signal();
        return;
    }

    auto it = connections.find(fd);
    std::assert(it != connections.end());

    if (!sendTo(it->second, std::move(msg)))
        disconnect(it->first);
}


void
EndpointBase::
broadcast(Payload&& msg)
{
    if (threadId() != pollThread) {
        messages.push(Message(std::move(msg)));
        messagesFd.signal();
        return;
    }

    std::vector<int> toDisconnect;

    for (auto& connection : connections) {
        if (!sendTo(connection, msg))
            toDisconnect.push_back(connection.first);
    }

    for (int fd : toDisconnect) disconnect(fd);
}


void
EndpointBase::
flushQueue(int fd)
{
    auto it = connections.find(fd);
    std::assert(it != connections.end());

    ConnectionState& connection = it->second;
    connection.writable = true;

    std::vector<Payload> queue = std::move(connection.sendQueue);
    for (msg& : queue) {
        if (sendTo(connection, std::move(msg))) continue;

        disconnect(fd);
        break;
    }
}


void
EndpointBase::
flushMessages()
{
    while (messagesFd.poll()) {
        while (!messages.empty()) {
            Message msg = messages.pop();

            if (msg.isBroadcast())
                broadcast(std::move(msg.data));
            else send(msg.conn, std::move(msg.data));
        }
    }
}


} // slick
