/* connections.tcc                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 25 Jan 2014
   FreeBSD-style copyright and disclaimer apply

   Implementation of the connections class.
*/

#include "connections.h"
#include "endpoint.h"
#include "utils.h"

#include <random>

namespace slick {


/******************************************************************************/
/* CONNECTIONS                                                                */
/******************************************************************************/


template<typename Data>
Connections<Data>::
Connections(Model model, Endpoint& endpoint, double period) :
    model(model),
    endpoint(endpoint),
    period_(calcPeriod(period)),
    timer(period_),
    rng(lockless::rdtsc())
{
    using namespace std::placeholders;
    timer.onTimer = std::bind(&Connections<Data>::onTimer, this, _1);
}

template<typename Data>
Connections<Data>::
~Connections()
{
}

template<typename Data>
void
Connections<Data>::
shutdown()
{
}

template<typename Data>
double
Connections<Data>::
calcPeriod(double value)
{
    size_t min = std::max<size_t>(1, base / 2);
    size_t max = min + base;
    size_t ms = std::uniform_int_distribution<size_t>(min, max)(rng);
    return double(ms) / 1000;
}

template<typename Data>
void
Connections<Data>::
period(double value)
{
    timer.setDelay(period_ = calcPeriod(value));
}

template<typename Data>
size_t
Connections<Data>::
peers() const
{
    return peers.size();
}

template<typename Data>
size_t
Connections<Data>::
connections() const
{
    return connections.size();
}

template<typename Data>
Peer&
Connections<Data>::
peer(size_t peerId)
{
    auto it = peers.find(peerId);
    assert(it != peers.end());
    return *it;
}

template<typename Data>
const Peer&
Connections<Data>::
peer(size_t peerId) const
{
    auto it = peers.find(peerId);
    assert(it != peers.end());
    return *it;
}

template<typename Data>
Connection&
Connections<Data>::
connection(size_t peerId)
{
    Peer& peer = peer(peerId);
    assert(peer.fd >= 0);

    auto it = connections.find(peer.fd);
    assert(it != connections.end());
    return *it;
}


template<typename Data>
const Connection&
Connections<Data>::
connection(size_t peerId) const
{
    Peer& peer = peer(peerId);
    assert(peer.fd >= 0);

    auto it = connections.find(peer.fd);
    assert(it != connections.end());
    return *it;
}

template<typename Data>
bool
Connections<Data>::
connected(size_t peerId) const
{
    return peer(peerId).connected();
}

template<typename Data>
Data&
Connections<Data>::
data(size_t peerId)
{
    return connection(peerId).data;
}


template<typename Data>
const Data&
Connections<Data>::
data(size_t peerId) const
{
    return connection(peerId).data;
}


template<typename Data>
const NodeAddress&
Connections<Data>::
addr(size_t peerId) const
{
    return peer(peerId).addr;
}


template<typename Data>
template<typename Payload>
void
Connections<Data>::
send(size_t peerId, Payload&& data)
{
    endpoint.send(connection(peerId).fd, std::forward<Payload>(data));
}

template<typename Data>
template<typename Payload>
void
Connections<Data>::
broadcast(Payload&& data);
{
    endpoint.multicast(broadcastFds, std::forward<Payload>(data));
}



template<typename Data>
bool
Connections<Data>::
test(int fd) const
{
    return connections.count(fd);
}

template<typename Data>
void
Connections<Data>::
poll(size_t timeoutMs)
{
    timer.poll(timeoutMs);
}


template<typename Data>
void
Connections<Data>::
connectPeer(const Peer& peer)
{
    assert(!peer.connected());

    peer.fd = endpoint.connect(peer.addr);
    assert(peer.connected()); // \todo need to handle this.

    connection[peer.fd] = Connection(peer.fd, peerId);
}

template<typename Data>
size_t
Connections<Data>::
add(NodeAddress addr)
{
    size_t peerId = ++peerIdCounter;

    Peer peer(peerId, std::move(addr));
    connectPeer(peer);
    peers[peerId] = std::move(peer);

    return peerId;
}

template<typename Data>
void
Connections<Data>::
remove(size_t peerId)
{
    Peer& peer = peer(peerId);

    if (peer.connected()) {
        endpoint.disconnect(peer.fd);
        broadcastFds.erase(peer.fd);
    }

    peers.erase(peerId);
}

template<typename Data>
void
Connections<Data>::
notifyConnect(int fd)
{
    Connection& conn = connections[fd];
    auto peerIt = peers.find(conn.peerId);
    if (peerIt == peers.end()) {
        endpoint.disconnect(conn.fd);
        return;
    }

    broadcastFds.insert(fd);
    peerIt->second.lastWaitMs = 0;

    if (model == Rotate) {
        size_t waitMs = period_ * 1000;
        waitMs *= 1 + std::geometric_distribution<size_t>(0.2)(rng);
        deadline.emplace_back(conn.peerId, waitMs);
    }

    if (onConnect) onConnect(conn.peerId);
}

template<typename Data>
void
Connections<Data>::
notifyDisconnect(int fd)
{
    auto it connIt = connections.find(fd);
    Connection conn = std::move(*connIt);
    connections.erase(connIt);
    broadcastFds.erase(conn.fd);

    auto peerIt = peers.find(conn.peerId);
    if (peerIt == peers.end()) return;

    peerIt->second.fd = -1;
    if (model == Persistent) {
        size_t& waitMs = peerIt->second.lastWaitMs;
        deadline.emplace(conn.peerId, waitMs);
        waitMs = waitMs ? waitMs * 2 : period_ * 1000;
    }

    if (onDisconnect)
        onDisconnect(conn.peerId);
}

template<typename Data>
void
Connections<Data>::
onTimer(uint64_t)
{
    double now = lockless::wall();
    while (deadlines.top().deadline <= now) {

        if (model == Persistent)
            reconnect(deadlines.front());
        else disconnect(deadline.front());

        deadline.pop();
    }

    if (model == Rotate) topupConnections(now);
}

template<typename Data>
void
Connections<Data>::
reconnect(const Deadline& deadline)
{
    auto peerIt = peers.find(deadline.peerId);
    if (peerIt == peers.end()) return;
    connectPeer(peerIt->second);
}

template<typename Data>
void
Connections<Data>::
disconnect(const Deadline& deadline)
{
    auto peerIt = peers.find(deadline.peerId);
    if (peerIt == peers.end()) return;
    if (!peerIt->second.connected()) return;

    endpoint.disconnect(peerIt->second.fd);
}

template<typename Data>
void
Connections<Data>::
topupConnections(double now)
{
    size_t targetSize = lockless::log2(peers.size());
    if (targetSize < connections.size()) return;

    size_t connects = targetSize - connections.size();
    while (connects) {
        auto peerIt = pickRandom(peers.begin(), peers.end(), rng);
        if (peerIt == peers.end()) break;

        connects--; // early increment prevents endless loops.

        if (!peerIt->second.connected()) break;
        connectPeer(peerIt->second);
    }
}


} // slick
