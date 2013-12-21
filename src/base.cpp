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

void
EndpointBase::
dropPayload(ConnectionHandle conn, Payload&& payload) const
{
    if (!onDroppedPayload) return;
    onDroppedPayload(conn, std::move(payload));
}


void
EndpointBase::
poll()
{
    pollThread = threadId(); // \todo This is a bit flimsy.

    while(poller.poll()) {

        struct epoll_event ev = poller.next();

        if (connections.count(ev.data.fd)) {

            if (ev.events & EPOLLERR)
                connections[ev.data.fd].socket.throwError();

            if (ev.events & EPOLLIN) {
                if (!recvPayload(ev.data.fd)) continue;
            }

            if (ev.events & EPOLLRDHUP || ev.events & EPOLLHUP) {
                disconnect(ev.data.fd);
                continue;
            }

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
processRecvBuffer(int fd, uint8_t* first, uint8_t* last)
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
        onPayload(fd, std::move(data));
    }

    assert(it == last);
    return first;
}

bool
EndpointBase::
recvPayload(int fd)
{
    auto conn = connections.find(fd);
    assert(conn != connections.end());

    enum { bufferLength = 1U << 16 };
    uint8_t buffer[bufferLength];
    uint8_t* bufferIt = buffer;

    while (true) {
        ssize_t read = recv(fd, bufferIt, (buffer + bufferLength) - bufferIt, 0);

        if (read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;
            SLICK_CHECK_ERRNO(read != -1, "EndpointBase.recv");
        }

        if (!read) { // indicates that shutdown was called on the connection side.
            disconnect(fd);
            return false;
        }

        conn->second.bytesRecv += read;
        bufferIt = processRecvBuffer(fd, buffer, bufferIt + read);
        assert(bufferIt < (buffer + bufferLength));
    }

    return true;
}


namespace {

// This is not part of the class EndpointBase because we don't want to make
// it part of the header.

template<typename Payload>
bool
sendTo(EndpointBase::ConnectionState& conn, Payload&& data, size_t offset = 0)
{
    if (!conn.writable) {
        conn.sendQueue.emplace_back(std::forward<Payload>(data), 0);
        return true;
    }

    const uint8_t* start = data.packet() + offset;
    ssize_t size = data.packetSize() - offset;
    assert(size > 0);

    while (true) {

        ssize_t sent = send(conn.socket.fd(), start, size, MSG_NOSIGNAL);
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

            conn.sendQueue.emplace_back(std::forward<Payload>(data), size);
            assert((conn.sendQueue.size() < 1U) << 16);
            return true;
        }

        else if (errno == ECONNRESET || errno == EPIPE) return false;
        SLICK_CHECK_ERRNO(sent >= 0, "EndpointBase.sendTo.send");
    }
}

} // namespace anonymous


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
    assert(it != connections.end());

    ConnectionState& connection = it->second;
    connection.writable = true;

    auto queue = std::move(connection.sendQueue);

    size_t i = 0;
    for (i = 0; i < queue.size(); ++i) {
        if (!sendTo(connection, std::move(queue[i].first), queue[i].second))
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

        dropPayload(op.conn, std::move(op.data));
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

        case Operation::Unicast: send(op.conn, std::move(op.data)); break;
        case Operation::Broadcast: broadcast(std::move(op.data)); break;

        case Operation::Connect: connect(std::move(op.connectSocket)); break;
        case Operation::Disconnect: disconnect(op.disconnectFd); break;

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
        if (socket.fd() < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;

        connect(std::move(socket));
    }
}



} // slick
