/* discovery.h                                  -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 24 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Endpoint discovery.
*/

#pragma once

#include "endpoint.h"
#include "poll.h"
#include "defer.h"
#include "timer.h"
#include "lockless/tm.h"

#include <string>
#include <functional>

namespace slick {

/******************************************************************************/
/* DISCOVERY                                                                  */
/******************************************************************************/

struct Discovery
{
    enum Event { New, Lost };
    typedef std::function<void(Event, Payload&&)> WatchFn;

    virtual void fd() = 0;
    virtual void poll() = 0;
    virtual void shutdown() {}

    virtual void discover(const std::string& key, const WatchFn& watch) = 0;
    virtual void retract(const std::string& key) = 0;
    virtual void publish(const std::string& key, Payload&& data) = 0;
    void publish(const std::string& key, const Payload& data)
    {
        publish(key, Payload(data));
    }


};


/******************************************************************************/
/* NODE LIST                                                                  */
/******************************************************************************/

struct NodeList
{
    typedef std::mt19937 RNG;

    bool test(const Address& addr) const;

    template<typename Address>
    bool add(Address&& addr)
    {
        if (test(addr)) return false;

        nodes.emplace_back(std::forward<Address>(addr));
        std::sort(nodes.begin(), nodes.end());

        return true;
    }

    Address pickRandom(RNG& rng) const;
    std::vector<Address> pickRandom(RNG& rng, size_t count) const;

private:
    std::vector<Address> nodes;
};


/******************************************************************************/
/* DISTRIBUTED DISCOVERY                                                      */
/******************************************************************************/

struct DistributedDiscovery : public Discovery
{
    enum { DefaultPort = 18888 };

    DistributedDiscovery(
            const std::vector<Address>& seed, Port port = DefaultPort);

    virtual int fd() const { return poller.fd(); }
    virtual void poll();
    virtual void shutdown();

    virtual void discover(const std::string& key, const WatchFn& watch);
    virtual void publish(const std::string& key, Payload&& data);
    virtual void retract(const std::string& key);

private:

    void onTimer(size_t);
    void onPayload(ConnectionHandle handle, Payload&& data);
    void onConnect(ConnectionHandle handle);
    void onDisconnect(ConnectionHandle handle);

    NodeList nodes;
    std::unordered_map<std::string, NodeList> keys;
    std::unordered_map<std::string, std::vector<WatchFn> > watches;
    std::unordered_map<std::string, Payload> data;

    struct ConnectionState
    {
        ConnectionState() :
            version(0), connectionTime(lockless::wall())
        {}

        size_t version;
        double connectionTime;
        std::vector<std::string> queries;
    };

    std::unordered_map<ConnectionHandle, ConnectionState> connections;


    SourcePoller poller;
    IsPollThread isPollThread;
    PassiveEndpoint endpoint;
    Timer timer;
    NodeList::RNG rng;

    enum { QueueSize = 1 << 4 };
    Defer<QueueSize, std::string> retracts;
    Defer<QueueSize, std::string, Payload> publishes;
    Defer<QueueSize, std::string, WatchFn> discovers;
};

} // slick
