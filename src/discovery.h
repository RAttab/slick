/* discovery.h                                  -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 24 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Endpoint discovery.

   \todo Fix the bajillion races with watch removal.
*/

#pragma once

#include "payload.h"
#include "uuid.h"
#include "poll.h"
#include "defer.h"

#include <string>
#include <functional>

namespace slick {

/******************************************************************************/
/* DISCOVERY                                                                  */
/******************************************************************************/

struct Discovery
{
    typedef size_t WatchHandle;
    typedef std::function<void(WatchHandle, const UUID&, const Payload&)> WatchFn;

    virtual WatchHandle discover(const std::string& key, const WatchFn& watch) = 0;
    virtual void forget(const std::string& key, WatchHandle handle) = 0;
    virtual void lost(const std::string& key, const UUID& keyId) = 0;

    virtual void retract(const std::string& key) = 0;
    virtual void publish(const std::string& key, Payload&& data) = 0;
    void publish(const std::string& key, const Payload& data)
    {
        publish(key, Payload(data));
    }

};


/******************************************************************************/
/* THREAD AWARE DISCOVERY                                                     */
/******************************************************************************/

struct ThreadAwareDiscovery : public Discovery, public ThreadAwarePollable
{
    virtual void stopPolling();

    virtual WatchHandle discover(const std::string& key, const WatchFn& watch);
    virtual void forget(const std::string& key, WatchHandle handle);
    virtual void lost(const std::string& key, const UUID& keyId);

    virtual void retract(const std::string& key);
    virtual void publish(const std::string& key, Payload&& data);

protected:

    void init(SourcePoller& poller);

    void discoverProxy(const std::string& key, WatchHandle handle, const WatchFn& watch);
    virtual void discoverImpl(const std::string& key, WatchHandle handle, const WatchFn& watch) = 0;
    virtual void forgetImpl(const std::string& key, WatchHandle handle) = 0;
    virtual void lostImpl(const std::string& key, const UUID& keyId) = 0;

    virtual void retractImpl(const std::string& key) = 0;
    virtual void publishImpl(const std::string& key, Payload&& data) = 0;

private:

    enum { QueueSize = 1 << 4 };
    Defer<QueueSize, std::string> retracts;
    Defer<QueueSize, std::string, Payload> publishes;
    Defer<QueueSize, std::string, WatchHandle, WatchFn> discovers;
    Defer<QueueSize, std::string, WatchHandle> forgets;
    Defer<QueueSize, std::string, UUID> losts;

    std::atomic<WatchHandle> watchCounter;
};

} // slick
