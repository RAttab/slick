/* discovery.h                                  -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 24 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Endpoint discovery.

   \todo Fix the bajillion races with watch removal.
*/

#pragma once

#include "endpoint.h"
#include "pack.h"
#include "poll.h"
#include "defer.h"
#include "timer.h"
#include "uuid.h"
#include "sorted_vector.h"
#include "lockless/tm.h"

#include <set>
#include <deque>
#include <string>
#include <functional>

namespace slick {

/******************************************************************************/
/* DISCOVERY                                                                  */
/******************************************************************************/

struct Discovery
{
    typedef size_t WatchHandle;
    typedef std::function<void(WatchHandle, const Payload&)> WatchFn;

    virtual int fd() const = 0;
    virtual void poll(size_t timeoutMs = 0) = 0;
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

        DefaultPeriod = 60 * 1,
        DefaultTTL    = 60 * 60 * 8,

        DefaultExpThresh = 10,
    };
    typedef std::vector<Address> NodeLocation;

    DistributedDiscovery(
            const std::vector<Address>& seed, Port port = DefaultPort);

    virtual int fd() const { return poller.fd(); }
    virtual void poll(size_t timeoutMs = 0);
    virtual void shutdown();

    virtual WatchHandle discover(const std::string& key, const WatchFn& watch);
    virtual void forget(const std::string& key, WatchHandle handle);

    virtual void publish(const std::string& key, Payload&& data);
    virtual void retract(const std::string& key);

    void ttl(size_t ttl = DefaultTTL) { ttl_ = ttl; }
    void connExpThresh(size_t sec = DefaultExpThresh) {connExpThresh_ = sec; }

    void setPeriod(size_t sec = DefaultPeriod);

    const UUID& id() const { return myId; }
    const NodeLocation& node() const { return myNode; }

private:

    typedef std::string QueryItem;
    typedef std::tuple<std::string, UUID> FetchItem;
    typedef std::tuple<std::string, Payload> DataItem;
    typedef std::tuple<UUID, NodeLocation, size_t> NodeItem;
    typedef std::tuple<std::string, UUID, NodeLocation, size_t> KeyItem;

    struct ConnState
    {
        int fd;
        size_t id; // sady, fds aren't unique so this is to dedup them.
        UUID nodeId;
        uint32_t version;
        std::vector<FetchItem> pendingFetch;

        ConnState();
        operator bool() const { return version; }
    };

    struct ConnExpItem
    {
        int fd;
        size_t id;
        double time;

        ConnExpItem(int fd = 0, size_t id = 0, double time = 0) :
            fd(fd), id(id), time(time)
        {}
    };

    struct Item
    {
        UUID id;
        NodeLocation addrs;
        double expiration;

        Item() : expiration(0) {}

        Item(KeyItem&& item, double now = lockless::wall()) :
            id(std::move(std::get<1>(item))),
            addrs(std::move(std::get<2>(item))),
            expiration(now + std::get<3>(item))
        {}

        Item(NodeItem&& item, double now = lockless::wall()) :
            id(std::move(std::get<0>(item))),
            addrs(std::move(std::get<1>(item))),
            expiration(now + std::get<2>(item))
        {}

        Item(UUID id, NodeLocation addrs, size_t ttl, double now = lockless::wall()) :
            id(std::move(id)), addrs(std::move(addrs)), expiration(now + ttl)
        {}

        size_t ttl(double now = lockless::wall()) const
        {
            if (expiration <= now) return 0;
            return expiration - now;
        }

        void setTTL(size_t ttl, double now = lockless::wall())
        {
            if (ttl > this->ttl(now))
            expiration = now + ttl;
        }

        bool operator<(const Item& other) const { return id < other.id; }
    };

    friend std::ostream& operator<<(std::ostream&, const Item&);


    struct Data
    {
        UUID id;
        Payload data;

        Data() {}
        explicit Data(Payload data) :
            id(UUID::random()), data(std::move(data))
        {}
    };


    struct Watch
    {
        WatchHandle handle;
        WatchFn watch;

        Watch(WatchHandle handle = 0) : handle(handle) {}
        Watch(WatchFn watch);
        bool operator< (const Watch& other) const
        {
            return handle < other.handle;
        }
    };


    size_t ttl_;
    size_t connExpThresh_;

    UUID myId;
    NodeLocation myNode;

    SortedVector<Item> nodes;
    std::unordered_map<UUID, int> connectedNodes;
    std::unordered_map<std::string, SortedVector<Item> > keys;
    std::unordered_map<std::string, std::set<Watch> > watches;
    std::unordered_map<std::string, Data> data;
    std::unordered_map<int, ConnState> connections;
    std::deque<ConnExpItem> connExpiration;

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


    size_t timerPeriod(size_t secs);
    void discover(const std::string& key, Watch&& watch);
    void onTimer(size_t);
    void onPayload(int fd, Payload&& data);
    void onConnect(int fd);
    void onDisconnect(int fd);

    ConstPackIt onInit (ConnState& conn, ConstPackIt first, ConstPackIt last);
    ConstPackIt onKeys (ConnState& conn, ConstPackIt first, ConstPackIt last);
    ConstPackIt onQuery(ConnState& conn, ConstPackIt first, ConstPackIt last);
    ConstPackIt onNodes(ConnState& conn, ConstPackIt first, ConstPackIt last);
    ConstPackIt onFetch(ConnState& conn, ConstPackIt first, ConstPackIt last);
    ConstPackIt onData (ConnState& conn, ConstPackIt first, ConstPackIt last);

    void doFetch(const std::string& key, const UUID& keyId, const NodeLocation& node);
    bool expireItem(SortedVector<Item>& list, double now);
    bool expireKeys(double now);
    void randomDisconnect(double now);
    void randomConnect(double now);

};

} // slick
