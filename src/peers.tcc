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
/* PEERS                                                                      */
/******************************************************************************/

template<typename Data>
Peers<Data>::
Peers(PeerModel model, Endpoint& endpoint, double period) :
    model(model),
    endpoint(endpoint),
    period_(calcPeriod(period)),
    timer(period_),
    idCounter(0),
    rng(lockless::rdtsc())
{
    using namespace std::placeholders;

    deadlines.onTimeout = std::bind(&Peers<Data>::onTimeout, this, _1);
    poller.add(deadlines);

    if (model == Rotate) {
        timer.onTimer = std::bind(&Peers<Data>::topupConnections, this);
        poller.add(timer);
    }
}

template<typename Data>
Peers<Data>::
~Peers()
{
}

template<typename Data>
void
Peers<Data>::
stopPolling()
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
PeerId
Peers<Data>::
id(int fd) const
{
    auto it = connections.find(fd);
    if (it == connetions.end()) return 0;
    return it->second.id;
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
connectionsTargetSize() const
{
    return model == Rotate ? lockless::log2(peers.size()) : peers.size();
}

template<typename Data>
void
Peers<Data>::
addRotateDeadline(PeerId id)
{
    size_t waitMs = period_ * 1000;
    waitMs *= 1 + std::geometric_distribution<size_t>(0.2)(rng);
    deadlines.setTTL(conn.id, waitMs);
}

template<typename Data>
void
Peers<Data>::
addReconnectDeadline(Peer& peer)
{
    size_t& waitMs = peer.lastWaitMs;
    deadlines.setTTL(peer.id, waitMs);
    waitMs = waitMs ? waitMs * 2 : period_ * 1000;
}


template<typename Data>
PeerId
Peers<Data>::
add(NodeAddress addr)
{
    PeerId id = ++idCounter;

    Peer peer(id, std::move(addr));

    if (connections.size() < connectionsTargetSize())
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
    deadlines.remove(id);
}

template<typename Data>
template<typename OtherData>
PeerId
Peers<Data>::
transfer(PeerId id, Peers<OtherData>& other)
{
    typedef Peers<OtherData>::Peer OtherPeer;
    typedef Peers<OtherData>::Connection OtherConnenction;

    Peer peer = std::move(peer(id));
    peers.erase(id);

    PeerId otherId = ++other.idCounter;
    auto ret = other.peers.emplace(otherId, Peer(otherId, std::move(peer.addr)));
    OtherPeer& otherPeer = *ret.first;

    if (peer.connected()) {
        otherPeer.fd = peer.fd;

        Connection conn = std::move(connetions[peer.fd]);
        connections.erase(peer.fd);

        auto ret = other.connections.emplace(peer.fd, OtherConnection(peer.fd, otherId));
        auto& otherConn = *ret.first;

        otherConn.data = OtherData(std::move(conn.data));
        if (model == Rotate) other.addRotateDeadline(otherId);
    }

    else if (connections.size() < connectionsTargetSize())
        other.connectPeer(otherPeer);

    return otherId;
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
    if (model == Rotate) addRotateDeadline(conn.id);

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
    if (model == Persistent) addReconnectDeadline(peerIt->second);

    if (onDisconnect)
        onDisconnect(conn.id);
}

template<typename Data>
void
Peers<Data>::
onTimeout(PeerId id)
{
    if (model == Persistent)
        reconnect(id);
    else disconnect(id);
}

template<typename Data>
void
Peers<Data>::
reconnect(PeerId id)
{
    auto peerIt = peers.find(id);
    assert(peerIt != peers.end());
    connectPeer(peerIt->second);
}

template<typename Data>
void
Peers<Data>::
disconnect(PeerId id)
{
    auto peerIt = peers.find(id);
    assert(peerIt != peers.end());
    if (!peerIt->second.connected()) return;

    endpoint.disconnect(peerIt->second.fd);
}

template<typename Data>
void
Peers<Data>::
topupConnections()
{
    if (model != Rotate) return;

    double now = lockless::wall();

    size_t targetSize = connectionsTargetSize();
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
