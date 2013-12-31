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

#include <set>
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
    typedef size_t WatchHandle;

    virtual void fd() = 0;
    virtual void poll() = 0;
    virtual void shutdown() {}

    virtual WatchHandle discover(const std::string& key, const WatchFn& watch) = 0;
    virtual void forget(const std::string& key, WatchHandle handle) = 0;

    virtual void retract(const std::string& key) = 0;
    virtual void publish(const std::string& key, Payload&& data) = 0;
    void publish(const std::string& key, const Payload& data)
    {
        publish(key, Payload(data));
    }

};


/******************************************************************************/
/* DISTRIBUTED DISCOVERY                                                      */
/******************************************************************************/

struct DistributedDiscovery : public Discovery
{
    enum {
        DefaultPort = 18888,

        DefaultKeyTTL = 60 * 10,
        DefaultNodeTTL = 60 * 60 * 8,

    };

    DistributedDiscovery(
            const std::vector<Address>& seed, Port port = DefaultPort);

    virtual int fd() const { return poller.fd(); }
    virtual void poll();
    virtual void shutdown();

    virtual WatchHandle discover(const std::string& key, const WatchFn& watch);
    virtual void forget(const std::string& key, WatchHandle handle);

    virtual void publish(const std::string& key, Payload&& data);
    virtual void retract(const std::string& key);

    void keyTTL(size_t ttl = DefaultKeyTTL) { keyTTL_ = ttl; }
    void nodeTTL(size_t ttl = DefaultNodeTTL) { nodeTTL_ = ttl; }

private:

    struct Watch;
    void discover(const std::string& key, Watch&& watch);

    void onTimer(size_t);
    void onPayload(ConnectionHandle handle, Payload&& data);
    void onConnect(ConnectionHandle handle);
    void onDisconnect(ConnectionHandle handle);

    size_t timerPeriod();

    struct ConnectionState
    {
        size_t version;
        double connectionTime;
        std::vector<std::string> queries;

        ConnectionState() :
            version(0), connectionTime(lockless::wall())
        {}
    };


    struct Node
    {
        std::vector<Address> addrs;
        double expiration;

        Node() : expiration(0) {}
        Node(std::vector<Address> addrs, size_t ttl, double now = lockless::wall()) :
            addrs(std::move(addrs)), expiration(now + ttl)
        {
            std::sort(addrs.begin(), addrs.end());
        }

        size_t ttl(double now = lockless::wall()) const
        {
            if (expiration <= now) return 0;
            return expiration - now;
        }

        bool operator<(const Node& other) const;
    };

    struct Watch
    {
        WatchHandle handle;
        WatchFn watch;

        Watch() : handle(0) {}
        Watch(WatchFn watch);
        bool operator< (const Watch& other) const
        {
            return handle < other.handle;
        }
    };

    template<typename T>
    struct List
    {
        bool count(const T& value) const;
        bool insert(T value);
        bool erase(const T& value) const;

        template<typename Rng>
        const T& pickRandom(Rng& rng) const
        {
            std::uniform_int_distribution<size_t> dist(0, list.size() - 1);
            return list[dist(rng)];
        }

        template<typename Rng>
        std::vector<T> pickRandom(Rng& rng, size_t count) const
        {
            assert(count < list.size());

            std::set<T> result;

            for (size_t i = 0; i < count; ++i)
                while (!result.insert(pickRandom(rng)).second);

            return std::vector<T>(result.begin(), result.end());
        }


    private:
        std::vector<Node> list;
    };

    size_t keyTTL_;
    size_t nodeTTL_;

    List<Node> nodes;
    std::unordered_map<std::string, List<Node> > keys;
    std::unordered_map<std::string, List<Watch> > watches;
    std::unordered_map<std::string, Payload> data;
    std::unordered_map<ConnectionHandle, ConnectionState> connections;

    std::mt19937 rng;

    SourcePoller poller;
    IsPollThread isPollThread;
    PassiveEndpoint endpoint;
    Timer timer;


    enum { QueueSize = 1 << 4 };
    Defer<QueueSize, std::string> retracts;
    Defer<QueueSize, std::string, Payload> publishes;
    Defer<QueueSize, std::string, Watch> discovers;
    Defer<QueueSize, std::string, WatchHandle> forgets;
};

} // slick
