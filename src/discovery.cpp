/* discovery.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 26 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Implementation of the node discovery database.

*/

#include "discovery.h"
#include "pack.h"
#include "stream.h"
#include "lockless/bits.h"

#include <set>
#include <vector>
#include <algorithm>
#include <functional>
#include <sstream>
#include <iostream>

namespace slick {

/******************************************************************************/
/* DEBUG                                                                      */
/******************************************************************************/

// for the friend crap to work this needs to be outside the anon namespace.
std::ostream& operator<<(
        std::ostream& stream, const DistributedDiscovery::Item& item)
{
    stream << "<";
    streamAll(stream, item.id, item.addrs, item.ttl());
    stream << ">";
    return stream;
}

namespace {

std::ostream& operator<<(std::ostream& stream, const Payload& data)
{
    stream << "<pl:" << data.size() << ">";
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const UUID& uuid)
{
    // stream << uuid.toString();
    stream << lockless::format("%08x", uuid.time_low);
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const Address& addr)
{
    stream << addr.toString();
    return stream;
}


template<typename... Args>
void print(UUID& id, const char* action, const Args&... args)
{
    std::stringstream ss;
    ss << id << ": " << action << "(";
    streamAll(ss, args...);
    ss << ")\n";
    std::cerr << ss.str();
}

} // namespace anonymous


/******************************************************************************/
/* UTILS                                                                      */
/******************************************************************************/

template<typename It, typename Rng>
It pickRandom(It it, It last, Rng& rng)
{
    if (it == last) return last;

    size_t dist = std::distance(it, last);
    size_t n = std::uniform_int_distribution<size_t>(0, dist - 1)(rng);
    std::advance(it, n);
    return it;
}

template<typename T, typename It, typename Rng>
std::set<T> pickRandom(It first, It last, size_t n, Rng& rng)
{
    if (first == last) return {};

    std::set<T> result;
    while (result.size() < n) {
        auto it = pickRandom(first, last, rng);
        if (it == last) break;
        result.insert(*it);
    }
    return std::move(result);
}


/******************************************************************************/
/* CONN STATE                                                                 */
/******************************************************************************/

DistributedDiscovery::ConnState::
ConnState() : fd(0), version(0), isFetch(false)
{
    static std::atomic<size_t> idCounter{0};
    id = ++idCounter;
}


/******************************************************************************/
/* WATCH                                                                      */
/******************************************************************************/

DistributedDiscovery::Watch::
Watch(WatchFn watch) : watch(std::move(watch))
{
    static std::atomic<WatchHandle> handleCounter{0};
    handle = ++handleCounter;
}


/******************************************************************************/
/* PROTOCOL                                                                   */
/******************************************************************************/

namespace Msg {

static const char* Init =  "_slick_disc_";
static constexpr uint32_t Version = 1;

typedef uint16_t Type;
static constexpr Type Keys  = 1;
static constexpr Type Query = 2;
static constexpr Type Nodes = 3;
static constexpr Type Fetch = 4;
static constexpr Type Data  = 5;

} // namespace Msg


/******************************************************************************/
/* DISTRIBUTED DISCOVERY                                                      */
/******************************************************************************/

DistributedDiscovery::
DistributedDiscovery(const std::vector<Address>& seeds, Port port) :
    ttl_(DefaultTTL),
    connExpThresh_(DefaultExpThresh),
    myId(UUID::random()),
    seeds(seeds),
    rng(lockless::wall()),
    endpoint(port),
    timer(timerPeriod(DefaultPeriod))
{
    myNode = networkInterfaces(true);
    for (auto& addr : myNode) addr.port = port;

    using namespace std::placeholders;

    endpoint.onPayload = bind(&DistributedDiscovery::onPayload, this, _1, _2);
    endpoint.onNewConnection = bind(&DistributedDiscovery::onConnect, this, _1);
    endpoint.onLostConnection = bind(&DistributedDiscovery::onDisconnect, this, _1);
    poller.add(endpoint);

    retracts.onOperation = std::bind(&DistributedDiscovery::retract, this, _1);
    poller.add(retracts);

    publishes.onOperation = std::bind(&DistributedDiscovery::publish, this, _1, _2);
    poller.add(publishes);

    typedef void (DistributedDiscovery::*DiscoverFn) (const std::string&, Watch&&);
    DiscoverFn discoverFn = &DistributedDiscovery::discover;
    discovers.onOperation = std::bind(discoverFn, this, _1, _2);
    poller.add(discovers);

    forgets.onOperation = std::bind(&DistributedDiscovery::forget, this, _1, _2);
    poller.add(forgets);

    timer.onTimer = bind(&DistributedDiscovery::onTimer, this, _1);
    poller.add(timer);
}

size_t
DistributedDiscovery::
timerPeriod(size_t base)
{
    size_t min = std::max<size_t>(1, base / 2);
    size_t max = min + base;
    return std::uniform_int_distribution<size_t>(min, max)(rng);
}

void
DistributedDiscovery::
setPeriod(size_t sec)
{
    timer.setDelay(timerPeriod(sec));
}

void
DistributedDiscovery::
poll(size_t timeoutMs)
{
    isPollThread.set();
    poller.poll(timeoutMs);
}

void
DistributedDiscovery::
shutdown()
{
    isPollThread.unset();
    retracts.poll();
    publishes.poll();
    discovers.poll();
    forgets.poll();
}


void
DistributedDiscovery::
onPayload(int fd, const Payload& data)
{
    auto connIt = connections.find(fd);
    assert(connIt != connections.end());
    auto& conn = connIt->second;


    auto it = data.cbegin(), last = data.cend();

    if (!conn.initialized()) it = onInit(conn, it, last);

    while (it != last) {
        Msg::Type type;
        it = unpack(type, it, last);

        switch(type) {
        case Msg::Keys:  it = onKeys(conn, it, last); break;
        case Msg::Query: it = onQuery(conn, it, last); break;
        case Msg::Nodes: it = onNodes(conn, it, last); break;
        case Msg::Fetch: it = onFetch(conn, it, last); break;
        case Msg::Data:  it = onData(conn, it, last); break;
        default: assert(false);
        }
    }
}


Discovery::WatchHandle
DistributedDiscovery::
discover(const std::string& key, const WatchFn& fn)
{
    Watch watch(fn);
    auto handle = watch.handle;
    discover(key, std::move(watch));
    return handle;
}

void
DistributedDiscovery::
discover(const std::string& key, Watch&& watch)
{
    if (!isPollThread()) {
        discovers.defer(key, watch);
        return;
    }

    print(myId, "wtch", key, watch.handle);

    if (!watches.count(key)) {
        std::vector<QueryItem> items = { key };
        print(myId, "brod", "qury", myNode, items);
        endpoint.broadcast(packAll(Msg::Query, myNode, items));
    }

    watches[key].insert(std::move(watch));

    auto it = keys.find(key);
    if (it == keys.end()) return;

    for (const auto& node : it->second)
        sendFetch(key, node.id, node.addrs);
}

void
DistributedDiscovery::
forget(const std::string& key, WatchHandle handle)
{
    if (!isPollThread()) {
        forgets.defer(key, handle);
        return;
    }

    auto it = watches.find(key);
    if (it == watches.end()) return;

    auto& list = it->second;
    list.erase(Watch(handle));

    if (list.empty())
        watches.erase(key);
}


void
DistributedDiscovery::
publish(const std::string& key, Payload&& data)
{
    if (!isPollThread()) {
        publishes.defer(key, std::move(data));
        return;
    }

    Data item(std::move(data));
    print(myId, "publ", key, item.id, item.data);

    std::vector<KeyItem> items;
    items.emplace_back(key, item.id, myNode, ttl_);

    print(myId, "brod", "keys", items);
    endpoint.broadcast(packAll(Msg::Keys, items));

    this->data[key] = std::move(item);
}


void
DistributedDiscovery::
retract(const std::string& key)
{
    if (!isPollThread()) {
        retracts.defer(key);
        return;
    }

    data.erase(key);
}


void
DistributedDiscovery::
onConnect(int fd)
{
    auto& conn = connections[fd];
    conn.fd = fd;
    connExpiration.emplace_back(fd, conn.id, lockless::wall());
    print(myId, "ocon", fd, conn.id, conn.isFetch, conn.nodeId);

    auto head = std::make_tuple(Msg::Init, Msg::Version, myId);
    Payload data;

    if (conn.pendingFetch.empty()) {
        print(myId, "send", "init", Msg::Version, myId);
        data = pack(head);
    }
    else {
        print(myId, "send", "init", Msg::Version, myId, "ftch", conn.pendingFetch);
        data = packAll(head, Msg::Fetch, conn.pendingFetch);
        conn.pendingFetch.clear();
    }

    endpoint.send(fd, std::move(data));
}


void
DistributedDiscovery::
onDisconnect(int fd)
{
    auto it = connections.find(fd);
    assert(it != connections.end());

    const auto& conn = it->second;
    print(myId, "disc", fd, conn.id, conn.nodeId, conn.version);

    connectedNodes.erase(conn.nodeId);
    connections.erase(it);
}


ConstPackIt
DistributedDiscovery::
onInit(ConnState& conn, ConstPackIt it, ConstPackIt last)
{
    std::string init;
    UUID nodeId;
    it = unpackAll(it, last, init, conn.version, nodeId);

    if (init != Msg::Init) {
        print(myId, "!err", "init-wrong-head", conn.fd, init);
        endpoint.disconnect(conn.fd);
        return last;
    }
    assert(conn.version == Msg::Version);

    print(myId, "recv", "init", conn.version, nodeId, it == last);

    if (!conn.nodeId) {
        conn.nodeId = nodeId;
        connectedNodes[nodeId] = conn.fd;
    }
    else if (nodeId != conn.nodeId) {
        print(myId, "!err", "init-wrong-id",
                conn.fd, nodeId.toString(), conn.nodeId.toString());
        endpoint.disconnect(conn.fd);
        return last;
    }

    // Fetch sockets can duplicate an existing link between two node so it's
    // specialized the fetch-data messages only.
    if (conn.isFetch) return it;
    if (it != last) {
        auto type = unpack<Msg::Type>(it, last);
        if (type == Msg::Fetch) return it;
    }

    int fd = conn.fd;
    sendInitQueries(fd);
    sendInitKeys(fd);
    sendInitNodes(fd);

    return it;
}

void
DistributedDiscovery::
sendInitQueries(int fd)
{
    if (watches.empty()) return;
    assert(connections.count(fd));

    std::vector<QueryItem> items;
    items.reserve(watches.size());

    for (const auto& watch : watches)
        items.emplace_back(watch.first);

    print(myId, "send", "qury", myNode, items);
    auto Msg = packAll(Msg::Query, myNode, items);
    endpoint.send(fd, std::move(Msg));
}

void
DistributedDiscovery::
sendInitKeys(int fd)
{
    if (data.empty()) return;
    assert(connections.count(fd));

    std::vector<KeyItem> items;
    items.reserve(data.size());

    for (const auto& key : data)
        items.emplace_back(key.first, key.second.id, myNode, ttl_);

    print(myId, "send", "keys", items);
    endpoint.send(fd, packAll(Msg::Keys, items));
}

void
DistributedDiscovery::
sendInitNodes(int fd)
{
    assert(connections.count(fd));

    double now = lockless::wall();
    size_t numPicks = lockless::log2(nodes.size());

    std::vector<NodeItem> items;
    items.reserve(numPicks + 1);
    items.emplace_back(myId, myNode, ttl_);

    auto picks = pickRandom<Item>(nodes.begin(), nodes.end(), numPicks, rng);
    for (const auto& node : picks) {
        size_t ttl = node.ttl(now);
        if (!ttl) continue;
        items.emplace_back(node.id, node.addrs, ttl);
    }

    print(myId, "send", "node", items);
    endpoint.send(fd, packAll(Msg::Nodes, items));
}


ConstPackIt
DistributedDiscovery::
onKeys(ConnState&, ConstPackIt it, ConstPackIt last)
{
    std::vector<KeyItem> items;
    it = unpack(items, it, last);

    print(myId, "recv", "keys", items);

    std::vector<KeyItem> toForward;
    toForward.reserve(items.size());

    double now = lockless::wall();

    for (auto& item : items) {
        std::string key = std::move(std::get<0>(item));
        Item value(std::move(item), now);

        auto& list = keys[key];

        auto it = list.find(value);

        if (it != list.end()) {
            // We don't want to let keys expire (duplicate watches) but we don't
            // want keys message to be spammed constantly in the network. So we
            // only forward keys when the ttl reaches its half-life.
            if (it->ttl(now) >= ttl_ / 2) continue;
            it->setTTL(value.ttl(now), now);
        }
        else {
            if (watches.count(key))
                sendFetch(key, value.id, value.addrs);
            list.insert(value);
        }

        toForward.emplace_back(key, value.id, value.addrs, value.ttl(now));
    }

    if (!toForward.empty()) {
        print(myId, "fwrd", "keys", toForward);
        endpoint.broadcast(packAll(Msg::Keys, toForward));
    }

    return it;
}


ConstPackIt
DistributedDiscovery::
onQuery(ConnState& conn, ConstPackIt it, ConstPackIt last)
{
    NodeLocation node;
    std::vector<QueryItem> items;
    it = unpackAll(it, last, node, items);

    print(myId, "recv", "qury", node, items);

    std::vector<KeyItem> reply;
    reply.reserve(items.size());

    double now = lockless::wall();

    for (const auto& key : items) {
        auto it = keys.find(key);
        if (it == keys.end()) continue;

        for (const auto& node : it->second) {
            if (!node.ttl(now)) continue;
            reply.emplace_back(key, node.id, node.addrs, node.ttl(now));
        }
    }

    if (!reply.empty()) {
        print(myId, "repl", "keys", reply);
        endpoint.send(conn.fd, packAll(Msg::Keys, reply));
    }

    return it;
}


ConstPackIt
DistributedDiscovery::
onNodes(ConnState&, ConstPackIt it, ConstPackIt last)
{
    std::vector<NodeItem> items;
    it = unpack(items, it, last);

    print(myId, "recv", "node", items);

    std::vector<NodeItem> toForward;
    toForward.reserve(items.size());

    double now = lockless::wall();

    for (auto& item : items) {
        Item value(std::move(item), now);
        if (value.id == myId) continue;

        auto it = nodes.find(value);
        if (it != nodes.end()) {
            it->setTTL(value.ttl(now), now);
            continue;
        }

        toForward.emplace_back(value.id, value.addrs, value.ttl(now));
        nodes.insert(std::move(value));
    }

    if (!toForward.empty()) {
        print(myId, "fwrd", "node", toForward);
        endpoint.broadcast(packAll(Msg::Nodes, toForward));
    }

    return it;
}


void
DistributedDiscovery::
sendFetch(const std::string& key, const UUID& keyId, const NodeLocation& node)
{
    auto socket = Socket::connect(node);
    if (!socket) return;

    int fd = socket.fd();
    print(myId, "conn", fd, node);

    assert(!connections.count(fd));
    connections[fd].fetch(key, keyId);
    endpoint.connect(std::move(socket));
}

ConstPackIt
DistributedDiscovery::
onFetch(ConnState& conn, ConstPackIt it, ConstPackIt last)
{
    std::vector<FetchItem> items;
    it = unpack(items, it, last);

    print(myId, "recv", "ftch", items);

    for (const auto& blah : data)
        print(myId, "kydb", blah.first, blah.second.id, blah.second.data);

    std::vector<DataItem> reply;
    reply.reserve(items.size());

    for (auto& item : items) {
        std::string key;
        UUID id;
        std::tie(key, id) = std::move(item);

        auto it = data.find(key);
        if (it == data.end() || it->second.id != id)
            continue;

        reply.emplace_back(key, it->second.data);
    }

    if (!reply.empty()) {
        print(myId, "repl", "data", reply);
        endpoint.send(conn.fd, packAll(Msg::Data, reply));
    }

    return last;
}


ConstPackIt
DistributedDiscovery::
onData(ConnState& conn, ConstPackIt it, ConstPackIt last)
{
    // Make sure we disconnect when we're done.
    auto connGuard = guard([&] { endpoint.disconnect(conn.fd); });

    std::vector<DataItem> items;
    it = unpack(items, it, last);

    print(myId, "recv", "data", items);

    for (auto& item : items) {
        std::string key;
        Payload payload;
        std::tie(key, payload) = std::move(item);

        auto it = watches.find(key);
        if (it == watches.end()) continue;

        // The copy is necessary because the callback could modify watches
        // (eg. call retract()) which would invalidate our iterator.
        auto toTrigger = it->second;
        for (const auto& watch : toTrigger)
            watch.watch(watch.handle, payload);
    }

    return last;
}


void
DistributedDiscovery::
onTimer(size_t)
{
    double now = lockless::wall();
    print(myId, "tick", size_t(now), nodes.size(), lockless::log2(nodes.size()));

    while(!nodes.empty() && expireItem(nodes, now));
    while(!keys.empty() && expireKeys(now));
    randomDisconnect(now);
    randomConnect(now);
    seedConnect(now);
}

bool
DistributedDiscovery::
expireItem(SortedVector<Item>& list, double now)
{
    assert(!list.empty());

    auto it = pickRandom(list.begin(), list.end(), rng);
    if (it->ttl(now)) return false;

    print(myId, "expr", it->id, it->ttl(now));
    list.erase(it);
    return true;
}


bool
DistributedDiscovery::
expireKeys(double now)
{
    assert(!keys.empty());

    auto it = pickRandom(keys.begin(), keys.end(), rng);
    if (!expireItem(it->second, now)) return false;

    if (it->second.empty()) keys.erase(it);
    return true;
}

void
DistributedDiscovery::
randomDisconnect(double now)
{
    if (connections.empty()) return;

    size_t targetSize = lockless::log2(nodes.size());
    size_t disconnects = lockless::log2(targetSize);
    disconnects = std::min(disconnects, connections.size());

    if (connections.size() - disconnects > targetSize)
        disconnects = connections.size() - targetSize;

    // Need to defer the call because the call could invalidate our connection
    // iterator through our onLostConnection callback.
    std::vector<int> toDisconnect;
    toDisconnect.reserve(disconnects);

    while(disconnects) {
        const auto& item = connExpiration.front();
        if (item.time + connExpThresh_ >= now) break;

        size_t id = item.id;
        int fd = item.fd;
        connExpiration.pop_front();

        auto it = connections.find(item.fd);
        if (it == connections.end() || it->second.id != id) continue;

        toDisconnect.push_back(fd);
        disconnects--;
    }

    if (!toDisconnect.empty())
        print(myId, "disc", toDisconnect);

    for (auto fd : toDisconnect)
        endpoint.disconnect(fd);
}

void
DistributedDiscovery::
randomConnect(double now)
{
    size_t targetSize = lockless::log2(nodes.size());
    if (targetSize < connections.size()) return;

    size_t connects = targetSize - connections.size();
    while (connects) {
        auto nodeIt = pickRandom(nodes.begin(), nodes.end(), rng);
        if (nodeIt == nodes.end()) break;
        if (!nodeIt->ttl(now)) continue;

        connects--; // early increment prevents endless loops.

        auto connIt = connectedNodes.find(nodeIt->id);;
        if (connIt != connectedNodes.end()) continue;

        auto socket = Socket::connect(nodeIt->addrs);
        int fd = socket.fd();
        if (!fd) continue;

        connectedNodes.emplace(nodeIt->id, fd);
        connections[fd].nodeId = nodeIt->id;

        print(myId, "rcon", fd, *nodeIt, connects);
        endpoint.connect(std::move(socket));
    }
}

void
DistributedDiscovery::
seedConnect(double)
{
    // \todo Should periodicatlly try to reconnect to this to heal partitions.
    if (!connections.empty()) return;

    for (size_t i = 0; i < seeds.size(); ++i) {
        auto socket = Socket::connect(seeds[i]);
        if (!socket.fd()) continue;

        print(myId, "seed", socket.fd(), seeds[i]);
        endpoint.connect(std::move(socket));
    }

}

} // slick
