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

        if (connections.count(ev.data.fd)) {

            if (ev.events & EPOLLERR)
                connections[ev.data.fd].socket.throwError();

            if (ev.events & EPOLLIN) recvPayload(ev.data.fd);

            if (ev.events & EPOLLRDHUP || ev.events & EPOLLHUP) {
                disconnect(ev.data.fd);
                continue;
            }

            if (ev.events & EPOLLOUT) flushQueue(ev.data.fd);
        }

        else if (ev.data.fd == messagesFd.fd()) {
            SLICK_CHECK_ERRNO(!(ev.events & EPOLLERR),
                    "EndpointBase.meesageFd.EPOLLERR");
            flushMessages();
        }

        else {
            onPollEvent(ev);
        }
    }
}


void
EndpointBase::
connect(Socket&& socket)
{
    poller.add(socket.fd(), EPOLLET | EPOLLIN | EPOLLOUT);

    int fd = socket.fd();

    ConnectionState connection;
    connection.socket = std::move(socket);
    connections[connection.socket.fd()] = std::move(connection);

    if (onNewConnection) onNewConnection(fd);
}


void
EndpointBase::
disconnect(int fd)
{
    poller.del(fd);
    connections.erase(fd);

    if (onLostConnection) onLostConnection(fd);
}


void
EndpointBase::
recvPayload(int fd)
{
    auto it = connections.find(fd);
    assert(it != connections.end());

    enum { bufferLength = 1U << 16 };
    uint8_t buffer[bufferLength];

    while (true) {
        ssize_t read = recv(fd, buffer, bufferLength, 0);
        if (read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;
            SLICK_CHECK_ERRNO(read != -1, "EndpointBase.recv");
        }

        assert(read < bufferLength);
        if (!read) { // indicates that shutdown was called on the connection side.
            disconnect(fd);
            break;
        }

        it->second.bytesRecv += read;
        onPayload(fd, Payload(buffer, read));
    }

}


namespace {

// This is not part of the class EndpointBase because we don't want to make
// it part of the header.

template<typename Payload>
bool sendTo(EndpointBase::ConnectionState& connection, Payload&& data)
{
    (void) connection;
    (void) data;

    if (!connection.writable) {
        connection.sendQueue.emplace_back(std::forward<Payload>(data));
        return true;
    }
    ssize_t sent =
        send(connection.socket.fd(), data.bytes(), data.size(), MSG_NOSIGNAL);
    if (sent >= 0) {
        assert(size_t(sent) == data.size());
        return true;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        connection.sendQueue.emplace_back(std::forward<Payload>(data));
        return true;
    }

    else if (errno == ECONNRESET || errno == EPIPE) return false;
    SLICK_CHECK_ERRNO(sent >= 0, "EndpointBase.sendTo.send");

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
    assert(it != connections.end());

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
        if (!sendTo(connection.second, msg))
            toDisconnect.push_back(connection.first);
    }

    for (int fd : toDisconnect) disconnect(fd);
}


void
EndpointBase::
flushQueue(int fd)
{
    auto it = connections.find(fd);
    assert(it != connections.end());

    ConnectionState& connection = it->second;
    connection.writable = true;

    std::vector<Payload> queue = std::move(connection.sendQueue);
    for (auto& msg : queue) {
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
