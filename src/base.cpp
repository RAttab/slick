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
    poller.add(operationsFd.fd());
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
shutdown()
{
    pollThread = 0;
    runOperations();
}

bool
EndpointBase::
isOffThread() const
{
    return pollThread && pollThread != threadId();
}

template<typename Payload>
void
EndpointBase::
dropPayload(ConnectionHandle conn, Payload&& data) const
{
    if (!onDroppedPayload) return;

    // This extra step ensures that any lvalue references are first copied
    // before being moved for the callback.
    Payload tmpData = std::forward<Payload>(data);

    onDroppedPayload(conn, std::move(tmpData));
}


void
EndpointBase::
poll(int timeoutMs)
{
    pollThread = threadId(); // \todo This is a bit flimsy.

    while(poller.poll(timeoutMs)) {

        struct epoll_event ev = poller.next();

        if (connections.count(ev.data.fd)) {

            if (ev.events & EPOLLERR) {
                auto& conn = connections[ev.data.fd];

                int err = conn.socket.error();
                if (err & EPOLLRDHUP || err & EPOLLHUP) {
                    disconnect(ev.data.fd);
                    continue;
                }

                conn.socket.throwError();
            }

            if (ev.events & EPOLLIN) recvPayload(ev.data.fd);
            if (ev.events & EPOLLOUT) flushQueue(ev.data.fd);
        }

        else if (ev.data.fd == operationsFd.fd()) {
            SLICK_CHECK_ERRNO(!(ev.events & EPOLLERR),
                    "EndpointBase.meesageFd.EPOLLERR");
            runOperations();
        }

        else onPollEvent(ev);
    }
}


void
EndpointBase::
connect(Socket&& socket)
{
    if (isOffThread()) {
        deferOperation(Operation(std::move(socket)));
        return;
    }

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
    if (isOffThread()) {
        deferOperation(Operation(fd));
        return;
    }

    auto it = connections.find(fd);
    assert(it != connections.end());

    if (onDroppedPayload) {
        for (auto& pl : it->second.sendQueue)
            dropPayload(fd, std::move(pl.first));
    }

    poller.del(fd);
    connections.erase(fd);

    if (onLostConnection) onLostConnection(fd);
}


uint8_t*
EndpointBase::
processRecvBuffer(ConnectionState& conn, uint8_t* first, uint8_t* last)
{
    uint8_t* it = first;

    while (it < last) {
        size_t leftover = last - it;
        Payload data = proto::fromBuffer(it, leftover);

        if (!data.packetSize()) {
            std::copy(it, last, first);
            return first + leftover;
        }

        it += data.packetSize();
        conn.recvQueue.emplace_back(std::move(data));
    }

    assert(it == last);
    return first;
}

void
EndpointBase::
recvPayload(int fd)
{
    auto connIt = connections.find(fd);
    assert(connIt != connections.end());

    auto& conn = connIt->second;
    conn.recvQueue.reserve(1 << 5);

    enum { bufferLength = 1U << 16 };
    uint8_t buffer[bufferLength];
    uint8_t* bufferIt = buffer;

    bool doDisconnect = false;

    while (true) {
        ssize_t read = recv(fd, bufferIt, (buffer + bufferLength) - bufferIt, 0);

        if (read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;
            SLICK_CHECK_ERRNO(read != -1, "EndpointBase.recv");
        }

        if (!read) { // indicates that shutdown was called on the client side.
            doDisconnect = true;
            break;
        }

        conn.bytesRecv += read;
        bufferIt = processRecvBuffer(conn, buffer, bufferIt + read);
        assert(bufferIt < (buffer + bufferLength));
    }

    for (auto& data : conn.recvQueue)
        onPayload(fd, std::move(data));
    conn.recvQueue.clear();;

    if (doDisconnect && connections.count(fd))
        disconnect(fd);
}


template<typename Payload>
void
EndpointBase::
pushToSendQueue(EndpointBase::ConnectionState& conn, Payload&& data, size_t offset)
{
    enum { MaxQueueSize = 1 << 8 };

    if (conn.sendQueue.size() < MaxQueueSize)
        conn.sendQueue.emplace_back(std::forward<Payload>(data), offset);

    else {
        dropPayload(conn.socket.fd(), std::forward<Payload>(data));
        stats.sendQueueFull++;
    }
}

template<typename Payload>
bool
EndpointBase::
sendTo(EndpointBase::ConnectionState& conn, Payload&& data, size_t offset)
{
    if (!conn.writable) {
        pushToSendQueue(conn, std::forward<Payload>(data), 0);
        return true;
    }

    const uint8_t* start = data.packet() + offset;
    ssize_t size = data.packetSize() - offset;
    assert(size > 0);

    while (true) {

        ssize_t sent = ::send(conn.socket.fd(), start, size, MSG_NOSIGNAL);
        assert(sent); // No idea what to do with a return value of 0.

        if (sent > 0) conn.bytesSent += sent;

        if (sent == size) return true;
        if (sent >= 0 && sent < size) {
            start += sent;
            size -= sent;
            assert(size > 0);
            continue;
        }

        // sent < 0

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            conn.writable = false;
            stats.writableOff++;

            size_t pos = data.packetSize() - size;
            pushToSendQueue(conn, std::forward<Payload>(data), pos);
            return true;
        }

        else if (errno == ECONNRESET || errno == EPIPE) return false;
        SLICK_CHECK_ERRNO(sent >= 0, "EndpointBase.sendTo.send");
    }
}


void
EndpointBase::
send(int fd, Payload&& data)
{
    if (isOffThread()) {
        deferOperation(Operation(fd, std::move(data)));
        return;
    }

    auto it = connections.find(fd);

    if (it == connections.end()) {
        dropPayload(fd, std::move(data));
        stats.sendToUnknown++;
        return;
    }

    if (!sendTo(it->second, std::move(data))) {
        dropPayload(fd, std::move(data));
        disconnect(it->first);
    }
}


void
EndpointBase::
broadcast(Payload&& data)
{
    if (isOffThread()) {
        deferOperation(Operation(std::move(data)));
        return;
    }

    std::vector<int> toDisconnect;

    for (auto& connection : connections) {
        if (!sendTo(connection.second, data))
            toDisconnect.push_back(connection.first);
    }

    for (int fd : toDisconnect) disconnect(fd);
}


void
EndpointBase::
flushQueue(int fd)
{
    auto it = connections.find(fd);
    if (it == connections.end()) return;

    auto& conn = it->second;
    conn.writable = true;
    stats.writableOn++;

    auto queue = std::move(conn.sendQueue);

    size_t i = 0;
    for (i = 0; i < queue.size(); ++i) {
        if (!sendTo(conn, std::move(queue[i].first), queue[i].second))
            break;
    }

    if (i == queue.size()) return;

    for (; i < queue.size(); ++i)
        dropPayload(fd, std::move(queue[i].first));

    disconnect(fd);
}

void
EndpointBase::
deferOperation(Operation&& op)
{
    assert(isOffThread());

    while (!operations.push(std::move(op))) {

        // non-payload ops are unlikely to be in a time-sensitive part of the
        // code so retrying is acceptable.
        // \todo Need something slightly better then spin-waiting.
        if (!op.isPayload()) continue;

        dropPayload(op.send.fd, std::move(op.send.data));
        stats.deferPayload++;
        return;
    }

    operationsFd.signal();
}


void
EndpointBase::
runOperations()
{
    assert(!isOffThread());

    while (operationsFd.poll());

    // The caps is required to keep the poll thread responsive when bombarded
    // with events.
    enum { OpsCap = 1 << 6 };

    for (size_t i = 0; !operations.empty() && i < OpsCap; ++i) {
        Operation op = operations.pop();

        switch(op.type) {

        case Operation::Unicast: send(op.send.fd, std::move(op.send.data)); break;
        case Operation::Broadcast: broadcast(std::move(op.send.data)); break;

        case Operation::Connect: connect(std::move(op.connect.socket)); break;
        case Operation::Disconnect: disconnect(op.disconnect.fd); break;

        default: assert(false);
        }

    }

    if (!operations.empty()) operationsFd.signal();
}


/******************************************************************************/
/* PASSIVE ENDPOINT BASE                                                      */
/******************************************************************************/


PassiveEndpointBase::
PassiveEndpointBase(Port port) :
    sockets(port, SOCK_NONBLOCK)
{
    for (int fd : sockets.fds())
        poller.add(fd, EPOLLET | EPOLLIN);
}


PassiveEndpointBase::
~PassiveEndpointBase()
{
    for (int fd : sockets.fds())
        poller.del(fd);
}


void
PassiveEndpointBase::
onPollEvent(struct epoll_event& ev)
{
    assert(ev.events == EPOLLIN);
    assert(sockets.test(ev.data.fd));

    while (true) {
        Socket socket = Socket::accept(ev.data.fd, SOCK_NONBLOCK);
        if (!socket) break;

        connect(std::move(socket));
    }
}



} // slick
