/* naming.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 30 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Naming service implementation
*/

#include "naming.h"

namespace slick {


/******************************************************************************/
/* LOCAL NAMING                                                               */
/******************************************************************************/

void
LocalNaming::
publish(const std::string& endpoint, Payload&& data)
{
    std::lock_guard<std::mutex> guard(lock);
    std::assert(!isDone);

    endpoints[endpoint].pendingEvents.emplace_back(New, std::move(data));
}

void
LocalNaming::
retract(const std::string& endpoint, Payload&& data)
{
    std::lock_guard<std::mutex> guard(lock);
    std::assert(!isDone);

    endpoints[endpoint].pendingEvents.emplace_back(Lost, std::move(data));
}

void
LocalNaming::
discover(const std::string& endpoint, const WatchFn& watch)
{
    std::lock_guard<std::mutex> guard(lock);
    std::assert(!isDone);

    endpoints[endpoint].pendingWatches.emplace_back(watch);
}

void
LocalNaming::
shutdown()
{
    std::lock_guard<std::mutex> guard(lock);
    isDone = true;
    endpoints.clear();
}

void
LocalNaming::
poll()
{
    std::lock_guard<std::mutex> guard(lock);
    if (isDone) return;

    for (auto& endpoint : endpoints) {
        auto& info = endpoint.second;

        for (auto& watch : info.pendingWatches) processWatch(info, watch);
        info.pendingWatches.clear();

        for (auto& event : info.pendingEvents) processEvent(info, event);
        info.pendingEvents.clear();
    }
}

void
LocalNaming::
processWatch(EndpointInfo& info, Watch& watch)
{
    for (const auto& data : info.payloads)
        watch(Payload(data));
    watches.emplace_back(std::move(watch));
}

void
LocalNaming::
processEvent(EndpointInfo& info, EventInfo& event)
{
    for (const auto& watch : info.watches)
        watch(event.type, Payload(event.data));

    if (event.type == New)
        payloads.emplace_back(std::move(event.data));

    else {
        auto it = std::find(
                info.payloads.begin(), info.payloads.end(), event.data);
        std::assert(it != info.payloads.end());

        info.payloads.erase(it);
    }
}


} // slick
