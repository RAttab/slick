/* peers.tcc                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 25 Jan 2014
   FreeBSD-style copyright and disclaimer apply

   Implementation of the peers class.
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
Peers<Data>::
Peers(PeerModel model, Endpoint& endpoint, double period) :
    model(model),
    endpoint(endpoint),
    period_(calcPeriod(period)),
    timer(period_),
    rng(lockless::rdtsc())
{
    using namespace std::placeholders;
    timer.onTimer = std::bind(&Peers<Data>::onTimer, this, _1);
}

template<typename Data>
Peers<Data>::
~Peers()
{
}

template<typename Data>
void
Peers<Data>::
shutdown()
{
}

template<typename Data>
double
Peers<Data>::
calcPeriod(double value)
{
    size_t min = std::max<size_t>(1, base / 2);
    size_t max = min + base;
    size_t ms = std::uniform_int_distribution<size_t>(min, max)(rng);
    return double(ms) / 1000;
}

template<typename Data>
void
Peers<Data>::
period(double value)
{
    timer.setDelay(period_ = calcPeriod(value));
}

template<typename Data>
size_t
Peers<Data>::
peers() const
{
    return peers.size();
}

template<typename Data>
size_t
Peers<Data>::
connections() const
{
    return connections.size();
}

template<typename Data>
Peer&
Peers<Data>::
peer(PeerId id)
{
    auto it = peers.find(id);
    assert(it != peers.end());
    return *it;
}

template<typename Data>
const Peer&
Peers<Data>::
peer(PeerId id) const
{
    auto it = peers.find(id);
    assert(it != peers.end());
    return *it;
}

template<typename Data>
Connection&
Peers<Data>::
connection(PeerId id)
{
    Peer& peer = peer(id);
    assert(peer.fd >= 0);

    auto it = connections.find(peer.fd);
    assert(it != connections.end());
    return *it;
}


template<typename Data>
const Connection&
Peers<Data>::
connection(PeerId id) const
{
    Peer& peer = peer(id);
    assert(peer.fd >= 0);

    auto it = connections.find(peer.fd);
    assert(it != connections.end());
    return *it;
}

template<typename Data>
bool
Peers<Data>::
connected(PeerId id) const
{
    return peer(id).connected();
}

template<typename Data>
Data&
Peers<Data>::
data(PeerId id)
{
    return connection(id).data;
}


template<typename Data>
const Data&
Peers<Data>::
data(PeerId id) const
{
    return connection(id).data;
}


template<typename Data>
const NodeAddress&
Peers<Data>::
addr(PeerId id) const
{
    return peer(id).addr;
}


template<typename Data>
template<typename Payload>
void
Peers<Data>::
send(PeerId id, Payload&& data)
{
    endpoint.send(connection(id).fd, std::forward<Payload>(data));
}

template<typename Data>
template<typename Payload>
void
Peers<Data>::
broadcast(Payload&& data);
{
    endpoint.multicast(broadcastFds, std::forward<Payload>(data));
}



template<typename Data>
bool
Peers<Data>::
test(int fd) const
{
    return connections.count(fd);
}

template<typename Data>
void
Peers<Data>::
poll(size_t timeoutMs)
{
    timer.poll(timeoutMs);
}


template<typename Data>
void
Peers<Data>::
connectPeer(const Peer& peer)
{
    assert(!peer.connected());

    peer.fd = endpoint.connect(peer.addr);
    assert(peer.connected()); // \todo need to handle this.

    connection[peer.fd] = Connection(peer.fd, id);
}

template<typename Data>
size_t
Peers<Data>::
add(NodeAddress addr)
{
    PeerId id = ++idCounter;

    Peer peer(id, std::move(addr));
    connectPeer(peer);
    peers[id] = std::move(peer);

    return id;
}

template<typename Data>
void
Peers<Data>::
remove(PeerId id)
{
    Peer& peer = peer(id);

    if (peer.connected()) {
        endpoint.disconnect(peer.fd);
        broadcastFds.erase(peer.fd);
    }

    peers.erase(id);
}

template<typename Data>
void
Peers<Data>::
notifyConnect(int fd)
{
    Connection& conn = connections[fd];
    auto peerIt = peers.find(conn.id);
    if (peerIt == peers.end()) {
        endpoint.disconnect(conn.fd);
        return;
    }

    broadcastFds.insert(fd);
    peerIt->second.lastWaitMs = 0;

    if (model == Rotate) {
        size_t waitMs = period_ * 1000;
        waitMs *= 1 + std::geometric_distribution<size_t>(0.2)(rng);
        deadline.emplace_back(conn.id, waitMs);
    }

    if (onConnect) onConnect(conn.id);
}

template<typename Data>
void
Peers<Data>::
notifyDisconnect(int fd)
{
    auto it connIt = connections.find(fd);
    Connection conn = std::move(*connIt);
    connections.erase(connIt);
    broadcastFds.erase(conn.fd);

    auto peerIt = peers.find(conn.id);
    if (peerIt == peers.end()) return;

    peerIt->second.fd = -1;
    if (model == Persistent) {
        size_t& waitMs = peerIt->second.lastWaitMs;
        deadline.emplace(conn.id, waitMs);
        waitMs = waitMs ? waitMs * 2 : period_ * 1000;
    }

    if (onDisconnect)
        onDisconnect(conn.id);
}

template<typename Data>
void
Peers<Data>::
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
Peers<Data>::
reconnect(const Deadline& deadline)
{
    auto peerIt = peers.find(deadline.id);
    if (peerIt == peers.end()) return;
    connectPeer(peerIt->second);
}

template<typename Data>
void
Peers<Data>::
disconnect(const Deadline& deadline)
{
    auto peerIt = peers.find(deadline.id);
    if (peerIt == peers.end()) return;
    if (!peerIt->second.connected()) return;

    endpoint.disconnect(peerIt->second.fd);
}

template<typename Data>
void
Peers<Data>::
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
