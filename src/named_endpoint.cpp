/* named_endpoint.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Implementation of the provider endpoint

   \todo Need to also trap the onError to clean up failed connections.
*/

#include "named_endpoint.h"
#include "pack.h"


namespace slick {

/******************************************************************************/
/* NAMED ENDPOINT                                                             */
/******************************************************************************/

NamedEndpoint::
NamedEndpoint(Discovery& discovery) :
    discovery(discovery)
{
    using namespace std::placeholders;

    Endpoint::onLostConnection = std::bind(&NamedEndpoint::onDisconnect, this, _1);
    poller.add(endpoint.fd());

    typedef void (NamedEndpoint::*ConnectFn)(const std::string&, FilterFn&&);
    connects.onOperation = std::bind((ConnectFn)&NamedEndpoint::connect, this,  _1, _2);
    poller.add(connects.fd());

    watches.onOperation = std::bind(&NamedEndpoint::onWatch, this, _1, _2, _3, _4);
    poller.add(watches.fd());
}

NamedEndpoint::
~NamedEndpoint()
{
}

void
NamedEndpoint::
poll(int timeoutMs)
{
    isPollThread.set();

    while (poller.poll(timeoutMs)) {

        struct epoll_event ev = poller.next();

        if      (ev.data.fd == connects.fd()) connects.poll();
        else if (ev.data.fd == watches.fd()) watches.poll();
        else endpoint.poll();
    }
}

void
NamedEndpoint::
stopPolling()
{
    Endpoint::stopPolling();

    if (!name.empty()) discovery.retract(name);
    for (auto watch : activeWatches)
        discovery.forget(watch.second.key, watch.first);
}

void
NamedEndpoint::
listen(std::string key, Port listenPort, Payload&& data)
{
    assert(!isPollThread.isPolling());

    if (!name.empty()) discovery.retract(name);

    name = std::move(key);
    endpoint.listen(listenPort);
    discovery.publish(name, packAll(networkInterfaces(true), data));
}

void
NamedEndpoint::
connect(const std::string& key, FilterFn&& filter)
{
    if (!isPollThread()) {
        connects.defer(key, std::move(filter));
        return;
    }

    using namespace std::placeholders;
    auto watchFn = std::bind(&NamedEndpoint::onWatch, this, key, _1, _2, _3);
    auto handle = discovery.discover(key, watchFn);

    activeWatches[handle] = Watch(key, std::move(filter));
}

void
NamedEndpoint::
onWatch(const std::string& key,
        Discovery::WatchHandle handle,
        const UUID& keyId,
        const Payload& data)
{
    if (!isPollThread()) {
        watches.defer(key, handle, keyId, data);
        return;
    }

    NodeAddress node;
    Payload filterData;
    unpackAll(data, node, filterData);

    const auto& watch = activeWatches[handle];
    if (watch.filter && !watch.filter(filterData)) return;

    int fd = Endpoint::connect(node);
    assert(fd > 0);
    connections[fd] = Connection(key, keyId);
}


void
NamedEndpoint::
onDisconnect(int fd)
{
    assert(isPollThread());

    auto it = connections.find(fd);
    assert(it != connections.end());

    discovery.lost(it->second.key, it->second.keyId);

    if (onLostConnection) onLostConnection(fd);
}

} // slick
