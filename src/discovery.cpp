/* discovery.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 26 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Implementation of the node discovery database.
*/

#include "discovery.h"

#include <algorithm>
#include <vector>
#include <functional>

namespace slick {


/******************************************************************************/
/* NODE LIST                                                                  */
/******************************************************************************/

bool
NodeList::
test(const Address& addr) const
{
    auto it = std::binary_search(nodes.begin(), nodes.end(), addr);
    return it != nodes.end();
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
/* DISTRIBUTED DISCOVERY                                                      */
/******************************************************************************/

DistributedDiscovery::
DistributedDiscovery(const std::vector<Address>& seed, Port port) :
    endpoint(port),
    timer(60) // \todo add randomness
{
    using namespace std::placeholders;

    endpoint.onPayload = bind(&DistributedDiscovery::onMessage, this, _1, _2);
    endpoint.onConnect = bind(&DistributedDiscovery::onConnect, this, _1);
    endpoint.onDisconnect = bind(&DistributedDiscovery::onDisconnect, this, _1);
    poller.add(endpoint);

    retracts.onOperation = std::bind(&DistributedDiscovery::retract, this, _1);
    poller.add(retracts);

    publishes.onOperation = std::bind(&DistributedDiscovery::publish, this, _1, _2);
    poller.add(publishes);

    discover.onOperation = std::bind(&DistributedDiscovery::discover, this, _1, _2);
    poller.add(discover);

    timer.onTimer = bind(&DistributedDiscovery::onTimer, this);
    poller.add(timer);
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
    opeations.poll(); // flush pending ops.
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
onPayload(ConnectionHandle handle, Payload&& data)
{

}

void
DistributedDiscovery::
onConnect(ConnectionHandle handle)
{

}

void
DistributedDiscovery::
onDisconnect(ConnectionHandle handle)
{

}


} // slick
