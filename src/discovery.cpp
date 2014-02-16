/* discovery.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 16 Feb 2014
   FreeBSD-style copyright and disclaimer apply

   Implementation details for the discovery interface.
*/

#include "discovery.h"

namespace slick {

Discovery::WatchHandle
ThreadAwareDiscovery::
discover(const std::string& key, const WatchFn& watch)
{
    WatchHandle handle = ++watchCounter;
    discoverProxy(key, handle, watch);
    return handle;
}

void
ThreadAwareDiscovery::
discoverProxy(const std::string& key, WatchHandle handle, const WatchFn& watch)
{
    if (!isPollThread()) {
        discovers.defer(key, handle, watch);
        return;
    }

    return discoverImpl(key, handle, watch);
}

void
ThreadAwareDiscovery::
forget(const std::string& key, WatchHandle handle)
{
    if (!isPollThread()) {
        forgets.defer(key, handle);
        return;
    }

    forgetImpl(key, handle);
}

void
ThreadAwareDiscovery::
lost(const std::string& key, const UUID& keyId)
{
    if (!isPollThread()) {
        losts.defer(key, keyId);
        return;
    }

    lostImpl(key, keyId);
}

void
ThreadAwareDiscovery::
retract(const std::string& key)
{
    if (!isPollThread()) {
        retracts.defer(key);
        return;
    }

    retractImpl(key);
}

void
ThreadAwareDiscovery::
publish(const std::string& key, Payload&& data)
{
    if (!isPollThread()) {
        publishes.defer(key, std::move(data));
        return;
    }

    publishImpl(key, std::move(data));
}

void
ThreadAwareDiscovery::
init(SourcePoller& poller)
{
    typedef ThreadAwareDiscovery Disc;
    using namespace std::placeholders;

    retracts.onOperation = std::bind(&Disc::retract, this, _1);
    poller.add(retracts);

    publishes.onOperation = std::bind(&Disc::publish, this, _1, _2);
    poller.add(publishes);

    discovers.onOperation = std::bind(&Disc::discoverProxy, this, _1, _2, _3);
    poller.add(discovers);

    forgets.onOperation = std::bind(&Disc::forget, this, _1, _2);
    poller.add(forgets);

    losts.onOperation = std::bind(&Disc::lost, this, _1, _2);
    poller.add(losts);
}


void
ThreadAwareDiscovery::
stopPolling()
{
    ThreadAwarePollable::stopPolling();
    retracts.poll();
    publishes.poll();
    discovers.poll();
    forgets.poll();
}


} // slick
