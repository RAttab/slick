/* endpoint.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 29 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Endpoint implementation.
*/

#include "endpoint.h"
#include "utils.h"
#include "lockless/tls.h"

#include <cassert>
#include <sys/epoll.h>

namespace slick {

/******************************************************************************/
/* ENDPOINT BASE                                                              */
/******************************************************************************/

Endpoint::
Endpoint()
{
    init();
}

Endpoint::
Endpoint(Port listenPort)
{
    init();

    listenSockets = PassiveSockets(listenPort);
    for (int fd : listenSockets.fds())
        poller.add(fd, EPOLLET | EPOLLIN);
}

void
Endpoint::
init()
{
    using namespace std::placeholders;

    poller.add(disconnectQueueFd.fd());

    typedef void (Endpoint::*SendFn) (int, Payload&&);
    sends.onOperation = std::bind((SendFn)&Endpoint::send, this, _1, _2);
    poller.add(sends.fd());

    typedef void (Endpoint::*BroadcastFn) (Payload&&);
    broadcasts.onOperation = std::bind((BroadcastFn)&Endpoint::broadcast, this, _1);
    poller.add(broadcasts.fd());

    typedef void (Endpoint::*ConnectFn) (Socket&&);
    connects.onOperation = std::bind((ConnectFn)&Endpoint::connect, this, _1);
    poller.add(connects.fd());

    typedef void (Endpoint::*DisconnectFn) (int);
    disconnects.onOperation = std::bind((DisconnectFn)&Endpoint::doDisconnect, this, _1);
    poller.add(disconnects.fd());

    onError = [=] (int, int errnum) {
        if (errnum == ECONNRESET || errnum == EPIPE) return true;

        auto errStr = checkErrnoString(errnum, "Endpoint.onError");
        throw std::runtime_error(errStr);
    };
}


Endpoint::
~Endpoint()
{
    // The extra step is required to not invalidate our iterator
    std::vector<int> toDisconnect;
    for (const auto& connection : connections)
        toDisconnect.push_back(connection.first);

    doDisconnect(std::move(disconnectQueue));
    for (int fd : toDisconnect)
        doDisconnect(fd);

    for (int fd : listenSockets.fds())
        poller.del(fd);
}

void
Endpoint::
shutdown()
{
    isPollThread.unset();

    doDisconnect(std::move(disconnectQueue));

    // flush any pending ops.
    sends.poll();
    broadcasts.poll();
    connects.poll();
    disconnects.poll();;
}

template<typename Payload>
void
Endpoint::
dropPayload(int fd, Payload&& data) const
{
    if (!onDroppedPayload) return;

    // This extra step ensures that any lvalue references are first copied
    // before being moved for the callback.
    Payload tmpData = std::forward<Payload>(data);

    onDroppedPayload(fd, std::move(tmpData));
}


void
Endpoint::
poll(int timeoutMs)
{
    isPollThread.set(); // \todo This is a bit flimsy

    while(poller.poll(timeoutMs)) {

        struct epoll_event ev = poller.next();

        if (connections.count(ev.data.fd)) {

            if (ev.events & EPOLLERR) {
                auto& conn = connections[ev.data.fd];

                int err = conn.socket.error();

                if (!err) continue;
                else if (!onError || onError(ev.data.fd, err))
                    disconnect(ev.data.fd);
            }

            if (ev.events & EPOLLIN) recvPayload(ev.data.fd);
            if (ev.events & EPOLLOUT) flushQueue(ev.data.fd);
        }

        else if (listenSockets.test(ev.data.fd)) accept(ev.data.fd);

        else if (ev.data.fd == disconnectQueueFd.fd())
            doDisconnect(std::move(disconnectQueue));

        else if (ev.data.fd == sends.fd())       sends.poll(DeferCap);
        else if (ev.data.fd == broadcasts.fd())  broadcasts.poll(DeferCap);
        else if (ev.data.fd == connects.fd())    connects.poll(DeferCap);
        else if (ev.data.fd == disconnects.fd()) disconnects.poll(DeferCap);

        else assert(false);
    }
}


void
Endpoint::
listen(Port listenPort)
{
    for (int fd : listenSockets.fds()) poller.del(fd);
    listenSockets = PassiveSockets(listenPort);
}

void
Endpoint::
accept(int fd)
{
    Socket socket;
    while (socket = Socket::accept(fd))
        connect(std::move(socket));
}

void
Endpoint::
connect(Socket&& socket)
{
    if (!isPollThread()) {
        connects.defer(std::move(socket));
        return;
    }

    int fd = socket.fd();
    poller.add(fd, EPOLLET | EPOLLIN | EPOLLOUT);

    auto it = connections.find(fd);
    assert(it == connections.end());

    ConnectionState connection;
    connection.socket = std::move(socket);
    connections[fd] = std::move(connection);

    if (onNewConnection) onNewConnection(fd);
}

int
Endpoint::
connect(const Address& addr)
{
    auto socket = Socket::connect(addr);
    if (!socket) return 0;

    int fd = socket.fd();
    connect(std::move(socket));
    return fd;
}

int
Endpoint::
connect(const std::vector<Address>& addrs)
{
    auto socket = Socket::connect(addrs);
    if (!socket) return 0;

    int fd = socket.fd();
    connect(std::move(socket));
    return fd;
}

void
Endpoint::
disconnect(int fd)
{
    if (!isPollThread.isPolling()) {
        doDisconnect(fd);
        return;
    }

    if (!isPollThread()) {
        disconnects.defer(fd);
        return;
    }

    auto it = connections.find(fd);
    if (it == connections.end() || it->second.dead)
        return;

    it->second.dead = true;

    disconnectQueue.emplace_back(fd);
    disconnectQueueFd.signal();
}

void
Endpoint::
doDisconnect(std::vector<int> fds)
{
    while (disconnectQueueFd.poll());
    for (int fd : fds) doDisconnect(fd);
}

void
Endpoint::
doDisconnect(int fd)
{
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
Endpoint::
processRecvBuffer(uint8_t* first, uint8_t* last, std::vector<Payload>& queue)
{
    uint8_t* it = first;

    while (it < last) {
        size_t leftover = last - it;
        Payload data = Payload::read(it, leftover);

        if (!data.packetSize()) {
            std::copy(it, last, first);
            return first + leftover;
        }

        it += data.packetSize();
        assert(data.packetSize());
        queue.emplace_back(std::move(data));
    }

    assert(it == last);
    return first;
}

void
Endpoint::
recvPayload(int fd)
{
    auto connIt = connections.find(fd);
    if (connIt == connections.end()) return;
    auto& conn = connIt->second;

    enum { bufferLength = 1U << 16 };
    uint8_t buffer[bufferLength];
    uint8_t* bufferIt = buffer;

    std::vector<Payload> queue;
    queue.reserve(1 << 5);

    bool doDisconnect = false;

    while (true) {
        ssize_t read = recv(fd, bufferIt, (buffer + bufferLength) - bufferIt, 0);

        if (read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;
            SLICK_CHECK_ERRNO(read != -1, "Endpoint.recv");
        }

        if (!read) { // indicates that shutdown was called on the client side.
            doDisconnect = true;
            break;
        }

        conn.bytesRecv += read;
        bufferIt = processRecvBuffer(buffer, bufferIt + read, queue);
        assert(bufferIt < (buffer + bufferLength));
    }

    for (auto& data : queue)
        onPayload(fd, std::move(data));

    if (doDisconnect && connections.count(fd))
        disconnect(fd);
}


template<typename Payload>
void
Endpoint::
pushToSendQueue(Endpoint::ConnectionState& conn, Payload&& data, size_t offset)
{
    enum { MaxQueueSize = 1 << 8 };

    if (conn.sendQueue.size() < MaxQueueSize)
        conn.sendQueue.emplace_back(std::forward<Payload>(data), offset);

    else dropPayload(conn.socket.fd(), std::forward<Payload>(data));
}

template<typename Payload>
bool
Endpoint::
sendTo(Endpoint::ConnectionState& conn, Payload&& data, size_t offset)
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

            size_t pos = data.packetSize() - size;
            pushToSendQueue(conn, std::forward<Payload>(data), pos);
            return true;
        }

        else if (errno == ECONNRESET || errno == EPIPE) return false;
        SLICK_CHECK_ERRNO(sent >= 0, "Endpoint.sendTo.send");
    }
}


void
Endpoint::
send(int fd, Payload&& data)
{
    if (!isPollThread()) {
        // \todo Need a way to avoid this copy.
        if (!sends.tryDefer(fd, data))
            dropPayload(fd, std::move(data));
        return;
    }

    auto it = connections.find(fd);

    if (it == connections.end()) {
        dropPayload(fd, std::move(data));
        return;
    }

    if (it->second.dead)
        dropPayload(fd, std::move(data));

    if (!sendTo(it->second, std::move(data))) {
        dropPayload(fd, std::move(data));
        disconnect(it->first);
    }
}


void
Endpoint::
broadcast(Payload&& data)
{
    if (!isPollThread()) {
        if (!broadcasts.tryDefer(data))
            dropPayload(-1, std::move(data));
        return;
    }

    std::vector<int> toDisconnect;

    for (auto& conn : connections) {
        if (conn.second.dead)
            dropPayload(conn.first, data);

        if (!sendTo(conn.second, data)) {
            dropPayload(conn.first, data);
            toDisconnect.push_back(conn.first);
        }
    }

    for (int fd : toDisconnect) disconnect(fd);
}


void
Endpoint::
flushQueue(int fd)
{
    auto it = connections.find(fd);
    if (it == connections.end()) return;

    auto& conn = it->second;
    conn.writable = true;

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

} // slick
