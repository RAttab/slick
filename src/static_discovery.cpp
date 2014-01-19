/* static_discovery.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 19 Jan 2014
   FreeBSD-style copyright and disclaimer apply

   static discovery implementation

   \todo gotta have key expiration...
*/

#include "static_discovery.h"
#include "discovery_utils.h"

namespace slick {


/******************************************************************************/
/* WATCH                                                                      */
/******************************************************************************/

StaticDiscovery::Watch::
Watch(WatchFn watch) : watch(std::move(watch))
{
    static std::atomic<WatchHandle> handleCounter{0};
    handle = ++handleCounter;
}


/******************************************************************************/
/* PROTOCOL                                                                   */
/******************************************************************************/

namespace Msg {

static const char* Init =  "_slick_static_disc_";
static constexpr uint32_t Version = 1;

typedef uint16_t Type;
static constexpr Type Keys  = 1;
static constexpr Type Query = 2;

} // namespace Msg


/******************************************************************************/
/* STATIC DISCOVERY                                                           */
/******************************************************************************/

StaticDiscovery::
StaticDiscovery(std::vector<Address> peers, Port port) :
    myId(UUID::random()),
    period_(timerPeriod(DefaultPeriod)),
    endpoint(port),
    timer(period_),
    peers(std::move(peers))
{
    endpoint.onPayload = bind(&StaticDiscovery::onPayload, this, _1, _2);
    endpoint.onNewConnection = bind(&StaticDiscovery::onConnect, this, _1);
    endpoint.onLostConnection = bind(&StaticDiscovery::onDisconnect, this, _1);
    poller.add(endpoint);

    retracts.onOperation = std::bind(&StaticDiscovery::retract, this, _1);
    poller.add(retracts);

    publishes.onOperation = std::bind(&StaticDiscovery::publish, this, _1, _2);
    poller.add(publishes);

    typedef void (StaticDiscovery::*DiscoverFn) (const std::string&, Watch&&);
    discovers.onOperation = std::bind(&StaticDiscovery::discover, this, _1, _2);
    poller.add(discovers);

    forgets.onOperation = std::bind(&StaticDiscovery::forget, this, _1, _2);
    poller.add(forgets);

    losts.onOperation = std::bind(&StaticDiscovery::lost, this, _1, _2);
    poller.add(losts);

    timer.onTimer = bind(&StaticDiscovery::onTimer, this, _1);
    poller.add(timer);
}

size_t
StaticDiscovery::
timerPeriod(size_t base)
{
    size_t min = std::max<size_t>(1, base / 2);
    size_t max = min + base;
    return std::uniform_int_distribution<size_t>(min, max)(rng);
}

void
StaticDiscovery::
period(size_t sec) const
{
    period_ = timerPeriod(sec);
}

void
StaticDiscovery::
poll(size_t timeoutMs)
{
    isPollThread.set();
    poller.poll(timeoutMs);
}

void
StaticDiscovery::
shutdown()
{
    isPollThread.unset();
    retracts.poll();
    publishes.poll();
    discovers.poll();
    forgets.poll();
}

void
StaticDiscovery::
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
        default: assert(false);
        }
    }

}


Discovery::WatchHandle
PeerDiscovery::
discover(const std::string& key, const WatchFn& fn)
{
    Watch watch(fn);
    auto handle = watch.handle;
    discover(key, std::move(watch));
    return handle;
}

void
StaticDiscovery::
discover(const std::string& key, Watch&& watch)
{
    if (!isPollThread()) {
        discovers.defer(key, std::move(watch));
    }

    print(myId, "wtch", key, watch.handle);

    if (!watches.count(key)) {
        std::vector<std::string> items = { key };
        print(myId, "brod", "qury", myNode, items);
        endpoint.multicast(edges, packAll(Msg::Query, items));
    }
    
    watches[key].insert(watch);

    auto it = keys.find(key);
    if (it == keys.end()) return;

    for (const auto& key : it->second)
        watch.watch(watch.handle, key.first, key.second);
}

void
StaticDiscovery::
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
StaticDiscovery::
lost(const std::string& key, const UUID& keyId)
{
    if (!isPollThread()) {
        losts.defer(key, keyId);
        return;
    }

    auto it = keys.find(key);
    if (it == keys.end()) return;

    auto& list = it->second;
    list.erase(Item(keyId));

    if (list.empty())
        keys.erase(key);
}

void
StaticDiscovery::
publish(const std::string& key, Payload&& data)
{
    assert(data);

    if (!isPollThread()) {
        publishes.defer(key, std::move(data));
        return;
    }
}

void
StaticDiscovery::
retract(const std::string& key)
{
    if (!isPollThread()) {
        retracts.defer(key);
        return;
    }

    data.erase(key);
}

void
StaticDiscovery::
onConnect(int fd)
{

}

void
StaticDiscovery::
onDisconnect(int fd)
{

}


ConstPackIt
StaticDiscovery::
onInit (ConnState& conn, ConstPackIt first, ConstPackIt last)
{

}

ConstPackIt
StaticDiscovery::
onQuery(ConnState& conn, ConstPackIt first, ConstPackIt last)
{

}

ConstPackIt
StaticDiscovery::
onKeys(ConnState& conn, ConstPackIt first, ConstPackIt last)
{

}

void
StaticDiscovery::
onTimer(size_t)
{

}



} // slick
