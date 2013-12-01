/* naming.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Naming service interface.
*/

#pragma once

#include "payload.h"

#include <functional>
#include <vector>
#include <map>
#include <mutex>

namespace slick {


/******************************************************************************/
/* NAMING                                                                     */
/******************************************************************************/

struct Naming
{
    enum Event { New, Lost };
    typedef std::function<void(Event, Payload&&)> WatchFn;

    virtual void poll() = 0;
    virtual void shutdown() = 0;

    virtual void publish(const std::string& endpoint, Payload&& data) = 0;
    virtual void retract(const std::string& endpoint, Payload&& data) = 0;
    virtual void discover(const std::string& endpoint, const WatchFn& watch) = 0;

    void retract(const std::string& endpoint, const Payload& data)
    {
        retract(endpoint, Payload(data));
    }

    void publish(const std::string& endpoint, const Payload& data)
    {
        publish(endpoint, Payload(data));
    }

};


/******************************************************************************/
/* LOCAL NAMING                                                               */
/******************************************************************************/

struct LocalNaming : public Naming
{
    LocalNaming() : isDone(false) {}
    virtual ~LocalNaming() { shutdown(); }

    virtual void poll();
    virtual void shutdown();

    virtual void publish(const std::string& endpoint, Payload&& data);
    virtual void retract(const std::string& endpoint, Payload&& data);
    virtual void discover(const std::string& endpoint, const WatchFn& watch);

private:

    struct EventInfo
    {
        EventInfo() {}
        EventInfo(Event type, Payload&& data) :
            type(type), data(std::move(data))
        {}

        Event type;
        Payload data;
    };

    struct EndpointInfo
    {
        std::vector<WatchFn> watches;
        std::vector<Payload> payloads;

        std::vector<WatchFn> pendingWathes;
        std::vector<EventInfo> pendingEvents;
    };

    void processWatch(EndpointInfo& info, WatchFn& watch);
    void processEvent(EndpointInfo& info, EventInfo& event);

    std::mutex lock;
    bool isDone;
    std::map<std::string, EndpointInfo> endpoints;
};

} // slick
