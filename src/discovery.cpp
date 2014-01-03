/* discovery.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 26 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Implementation of the node discovery database.

*/

#include "discovery.h"
#include "pack.h"
#include "lockless/bits.h"

#include <set>
#include <vector>
#include <algorithm>
#include <functional>

namespace slick {


/******************************************************************************/
/* UTILS                                                                      */
/******************************************************************************/

template<typename It, typename Rng>
It pickRandom(It it, It last, Rng& rng)
{
    std::uniform_int_distribution<size_t> dist(std::distance(it, last) - 1);
    std::advance(it, dist(rng));
    return it;
}

template<typename T, typename It, typename Rng>
std::set<T> pickRandom(It first, It last, size_t n, Rng& rng)
{
    std::set<T> result;
    while (result.size() < n)
        result.insert(*pickRandom(first, last, rng));
    return std::move(result);
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

static const std::string Init =  "_slick_disc_";
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
DistributedDiscovery(const std::vector<Address>& seed, Port port) :
    keyTTL_(DefaultKeyTTL),
    nodeTTL_(DefaultNodeTTL),
    myId(UUID::random()),
    rng(lockless::wall()),
    endpoint(port),
    timer(timerPeriod(DefaultPeriod))
{
    myNode = networkInterfaces(true);

    double now = lockless::wall();
    for (auto& addr : seed)
        nodes.emplace(UUID::random(), NodeLocation({addr}), SeedTTL, now);

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
    size_t min = base / 2, max = min + base;
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
poll()
{
    isPollThread.set();
    poller.poll();
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
onPayload(int fd, Payload&& data)
{
    auto& conn = connections[fd];
    auto it = data.cbegin(), last = data.cend();

    if (!conn) it = onInit(conn, it, last);

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
        };
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

    if (!watches.count(key)) {
        std::vector<QueryItem> items = { key };
        endpoint.broadcast(packAll(Msg::Query, myNode, items));
    }

    watches[key].insert(std::move(watch));

    for (const auto& node : keys[key])
        doFetch(key, node.id, node.addrs);
}

void
DistributedDiscovery::
forget(const std::string& key, WatchHandle handle)
{
    if (!isPollThread()) {
        forgets.defer(key, handle);
        return;
    }

    auto& list = watches[key];
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

    this->data[key] = Data(std::move(data));

    std::vector<KeyItem> items;
    items.emplace_back(key, myId, myNode, keyTTL_);

    endpoint.broadcast(packAll(Msg::Keys, items));
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

    auto head = std::make_tuple(Msg::Init, Msg::Version);

    Payload data;

    if (conn.pendingFetch.empty()) data = pack(head);
    else {
        data = packAll(head, Msg::Fetch, conn.pendingFetch);
        conn.pendingFetch.clear();
    }

    endpoint.send(fd, std::move(data));
}


void
DistributedDiscovery::
onDisconnect(int fd)
{
    connections.erase(fd);
}


ConstPackIt
DistributedDiscovery::
onInit(ConnState& conn, ConstPackIt it, ConstPackIt last)
{
    std::string init;
    it = unpackAll(it, last, init, conn.version);

    if (init != Msg::Init) {
        endpoint.disconnect(conn.fd);
        return last;
    }

    assert(conn.version == Msg::Version);

    if (!data.empty()) {
        std::vector<KeyItem> items;
        items.reserve(data.size());

        for (const auto& key : data)
            items.emplace_back(key.first, key.second.id, myNode, keyTTL_);

        endpoint.send(conn.fd, packAll(Msg::Keys, items));
    }

    if (!watches.empty()) {
        std::vector<QueryItem> items;
        items.reserve(watches.size());

        for (const auto& watch : watches)
            items.emplace_back(watch.first);

        auto Msg = packAll(Msg::Query, myNode, items);
        endpoint.send(conn.fd, std::move(Msg));
    }

    if (!nodes.empty()) {
        double now = lockless::wall();
        size_t numPicks = lockless::log2(nodes.size());

        std::vector<NodeItem> items;
        items.reserve(numPicks + 1);
        items.emplace_back(myId, myNode, nodeTTL_);

        auto picks = pickRandom<Item>(nodes.begin(), nodes.end(), numPicks, rng);
        for (const auto& node : picks) {
            size_t ttl = node.ttl(now);
            if (!ttl) continue;
            items.emplace_back(node.id, node.addrs, ttl);
        }

        endpoint.send(conn.fd, packAll(Msg::Nodes, items));
    }

    return it;
}


ConstPackIt
DistributedDiscovery::
onKeys(ConnState&, ConstPackIt it, ConstPackIt last)
{
    std::vector<KeyItem> items;
    it = unpack(items, it, last);

    std::vector<KeyItem> toForward;
    toForward.reserve(items.size());

    double now = lockless::wall();

    for (auto& item : items) {
        std::string key = std::move(std::get<0>(item));
        Item value(std::move(item), now);

        auto& list = keys[key];

        auto it = list.find(value);
        if (it != list.end()) {
            it->setTTL(value.ttl(now), now);
            continue;
        }

        if (watches.count(key)) doFetch(key, value.id, value.addrs);
        toForward.emplace_back(key, value.id, value.addrs, value.ttl(now));

        list.insert(std::move(value));
    }

    if (!toForward.empty())
        endpoint.broadcast(packAll(Msg::Keys, toForward));

    return it;
}


ConstPackIt
DistributedDiscovery::
onQuery(ConnState& conn, ConstPackIt it, ConstPackIt last)
{
    NodeLocation node;
    std::vector<QueryItem> items;
    it = unpackAll(it, last, node, items);

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

    if (!reply.empty())
        endpoint.send(conn.fd, packAll(Msg::Keys, reply));

    return it;
}


ConstPackIt
DistributedDiscovery::
onNodes(ConnState&, ConstPackIt it, ConstPackIt last)
{
    std::vector<NodeItem> items;
    it = unpack(items, it, last);

    std::vector<NodeItem> toForward;
    toForward.reserve(items.size());

    double now = lockless::wall();

    for (auto& item : items) {
        Item value(std::move(item), now);

        auto it = nodes.find(value);
        if (it != nodes.end()) {
            it->setTTL(value.ttl(now), now);
            continue;
        }

        toForward.emplace_back(value.id, value.addrs, value.ttl(now));
        nodes.insert(std::move(value));
    }

    if (!toForward.empty())
        endpoint.broadcast(packAll(Msg::Nodes, toForward));

    return it;
}


void
DistributedDiscovery::
doFetch(const std::string& key, const UUID& id, const NodeLocation& node)
{
    auto socket = Socket::connect(node);
    if (!socket) return;

    int fd = socket.fd();
    connections[fd].pendingFetch.emplace_back(key, id);
    endpoint.connect(std::move(socket));
}

ConstPackIt
DistributedDiscovery::
onFetch(ConnState& conn, ConstPackIt it, ConstPackIt last)
{
    std::vector<FetchItem> items;
    it = unpack(items, it, last);

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

    if (!reply.empty())
        endpoint.send(conn.fd, packAll(Msg::Data, reply));

    return it;
}


ConstPackIt
DistributedDiscovery::
onData(ConnState&, ConstPackIt it, ConstPackIt last)
{
    std::vector<DataItem> items;
    it = unpack(items, it, last);

    for (auto& item : items) {
        std::string key;
        Payload payload;
        std::tie(key, payload) = std::move(item);

        auto it = watches.find(key);
        if (it == watches.end()) continue;

        for (const auto& watch : it->second)
            watch.watch(watch.handle, payload);
    }

    return it;
}


void
DistributedDiscovery::
onTimer(size_t)
{
    double now = lockless::wall();

    while(!nodes.empty() && expireItem(nodes, now));
    while(!keys.empty() && expireKeys(now));
    rotateConnections();
}

bool
DistributedDiscovery::
expireItem(SortedVector<Item>& list, double now)
{
    assert(!list.empty());

    auto it = pickRandom(list.begin(), list.end(), rng);
    if (it->ttl(now)) return false;

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
rotateConnections()
{
    size_t targetSize = lockless::log2(nodes.size());
    size_t disconnects = lockless::log2(targetSize);

    if (connections.size() - disconnects > targetSize)
        disconnects = connections.size() - targetSize;

    std::set<int> toDisconnect;

    if (disconnects >= connections.size()) {
        for (const auto& conn : connections)
            toDisconnect.insert(conn.second.fd);
    }
    else {
        for (size_t i = 0; i < disconnects; ++i) {
            std::uniform_int_distribution<size_t> dist(0, connections.size() -1);

            auto it = connections.begin();
            std::advance(it, dist(rng));

            toDisconnect.insert(it->second.fd);
        }
    }

    // Need to defer the call because the call could invalidate our connection
    // iterator through our onLostConnection callback.
    for (auto fd : toDisconnect)
        endpoint.disconnect(fd);

    size_t connects = targetSize - connections.size();
    auto picks = pickRandom<Item>(nodes.begin(), nodes.end(), connects, rng);
    for (const auto& node : picks)
        endpoint.connect(node.addrs);
}

} // slick
