/* discovery.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 26 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Implementation of the node discovery database.

   PROTOCOL:

   onConnect:
       send(HELLO)

   onDiscover(key):
       watches.add(key)
       broadcast(WATCHING[node -> node])
       if (keys.has(key))
           send(GET(key -> node)

   onPublish(key, data):
        data.add(key -> data)
        broadcast(KEY[key, node, ttl])

   recv(HELLO):
       send(KEY[data.keys(), me()])
       send(WANT[watches.keys(), me()])
       send(NODES[nodes.rnd() -> node, ttl])

   recv(KEY[key, node, ttl]):
       if (keys.has(key, node)) return

       keys.add(key -> node, ttl)
       broadcast(KEY[key, node, ttl])

       if (watches.has(key))
           send(node, GET[key])

   recv(WANT[key, node]):
       if (keys.has(key) -> node, ttl)
           reply(KEY[key, node ttl])
           send(node, KEY[key, node, ttl])
       else
           keys.add(key -> [])
           broadcast(WANT[key, node])

    recv(GET[key]):
        reply(DATA[key, data.get(key)])

    recv(DATA[key, data]):
        watches.trigger(data)

    recv(NODES[node, ttl]):
        nodes.add(node -> ttl)

    \todo Need some kind of decay mechanism for the keys structure. Otherwise
    stale keys will remain forever.

*/

#include "discovery.h"
#include "pack.h"

#include <set>
#include <vector>
#include <algorithm>
#include <functional>

namespace slick {


/******************************************************************************/
/* ADDRESS                                                                    */
/******************************************************************************/

template<>
struct Pack<Address>
{
    static size_t size(const Address& value)
    {
        return packedSizeAll(value.host, value.port);
    }

    static void pack(const Address& value, PackIt first, PackIt last)
    {
        packAll(first, last, value.host, value.port);
    }

    static Address unpack(ConstPackIt first, ConstPackIt last)
    {
        Address value;
        unpackAll(first, last, value.host, value.port);
        return std::move(value);
    }
};


/******************************************************************************/
/* NODE                                                                       */
/******************************************************************************/

bool
DistributedDiscovery::Node::
operator<(const Node& other) const
{
    size_t n = std::min(addrs.size(), other.addrs.size());
    for (size_t i = 0; i < n; ++i) {
        if (addrs[i] < other.addrs[i]) return true;
        if (other.addrs[i] < addrs[i]) return false;
    }

    if (addrs.size() < other.addrs.size()) return true;
    if (other.addrs.size() < addrs.size()) return false;

    return expiration < other.expiration;
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
/* LIST                                                                       */
/******************************************************************************/

template<typename T>
bool
DistributedDiscovery::List<T>::
count(const T& value) const
{
    auto res = std::equal_range(list.begin(), list.end(), value);
    return res.first != res.second;
}

template<typename T>
bool
DistributedDiscovery::List<T>::
insert(T value)
{
    if (count(value)) return false;

    list.emplace_back(std::move(value));
    std::sort(list.begin(), list.end());

    return true;
}

template<typename T>
bool
DistributedDiscovery::List<T>::
erase(const T& value) const
{
    auto res = std::equal_range(list.begin(), list.end(), value);
    if (res.first == res.second) return false;

    list.erase(res.first);
}


/******************************************************************************/
/* DISTRIBUTED DISCOVERY                                                      */
/******************************************************************************/

DistributedDiscovery::
DistributedDiscovery(const std::vector<Address>& seed, Port port) :
    keyTTL_(DefaultKeyTTL),
    nodeTTL_(DefaultNodeTTL),
    endpoint(port),
    timer(60) // \todo add randomness
{
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

    for (auto& addr : seed)
        nodes.insert(Node({ addr }, nodeTTL_));
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

    return;
}

void
DistributedDiscovery::
forget(const std::string& key, WatchHandle handle)
{
    if (!isPollThread()) {
        forgets.defer(key, handle);
        return;
    }

}


void
DistributedDiscovery::
publish(const std::string& key, Payload&& data)
{
    if (!isPollThread()) {
        publishes.defer(key, std::move(data));
        return;
    }

}

void
DistributedDiscovery::
retract(const std::string& key)
{
    if (!isPollThread()) {
        retracts.defer(key);
        return;
    }

}

void
DistributedDiscovery::
onTimer(size_t)
{

}


void
DistributedDiscovery::
onPayload(ConnectionHandle handle, Payload&& data)
{
    (void) handle;
    (void) data;
}

void
DistributedDiscovery::
onConnect(ConnectionHandle handle)
{
    (void) handle;
}

void
DistributedDiscovery::
onDisconnect(ConnectionHandle handle)
{
    connections.erase(handle);
}


} // slick
