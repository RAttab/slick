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
/* NODE LIST                                                                  */
/******************************************************************************/

bool
NodeList::
test(const Address& addr) const
{
    auto res = std::equal_range(nodes.begin(), nodes.end(), addr);
    return res.first != res.second;
}

Address
NodeList::
pickRandom(RNG& rng) const
{
    std::uniform_int_distribution<size_t> dist(0, nodes.size() - 1);
    return nodes[dist(rng)];
}

std::vector<Address>
NodeList::
pickRandom(RNG& rng, size_t count) const
{
    assert(count < nodes.size());

    std::set<Address> result;

    for (size_t i = 0; i < count; ++i)
        while (!result.insert(pickRandom(rng)).second);

    return std::vector<Address>(result.begin(), result.end());
}


/******************************************************************************/
/* PROTOCOL                                                                   */
/******************************************************************************/

namespace {

static constexpr size_t ProtocolVersion = 1;

namespace msg {

std::tuple<std::string, size_t>
handshake()
{
    return std::make_tuple(std::string("_disc_"), ProtocolVersion);
}

typedef decltype(handshake()) HandshakeT;


} // namespace msg
} // namespace anonymous


/******************************************************************************/
/* DISTRIBUTED DISCOVERY                                                      */
/******************************************************************************/

DistributedDiscovery::
DistributedDiscovery(const std::vector<Address>& seed, Port port) :
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

    discovers.onOperation = std::bind(&DistributedDiscovery::discover, this, _1, _2);
    poller.add(discovers);

    timer.onTimer = bind(&DistributedDiscovery::onTimer, this, _1);
    poller.add(timer);

    for (auto& addr : seed)
        nodes.add(std::move(addr));
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
}

void
DistributedDiscovery::
discover(const std::string& key, const WatchFn& watch)
{
    if (!isPollThread()) {
        discovers.defer(key, watch);
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
    endpoint.send(handle, pack(msg::handshake()));
}

void
DistributedDiscovery::
onDisconnect(ConnectionHandle handle)
{
    connections.erase(handle);
}


} // slick
