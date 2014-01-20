/* peer_discovery.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 19 Jan 2014
   FreeBSD-style copyright and disclaimer apply

   p2p discovery implementation.
*/

#pragma once

#include "discovery.h"
#include "endpoint.h"
#include "pack.h"
#include "poll.h"
#include "defer.h"
#include "timer.h"
#include "sorted_vector.h"
#include "lockless/tm.h"

#include <set>
#include <map>
#include <deque>
#include <string>


namespace slick {

/******************************************************************************/
/* PEER DISCOVERY                                                             */
/******************************************************************************/

struct PeerDiscovery : public Discovery
{
    enum {
        DefaultPort = 18888,

        DefaultPeriod = 1000 * 60 * 1,
        DefaultTTL    = 1000 * 60 * 60 * 8,

        DefaultExpThresh = 1000 * 10,
    };
    typedef std::vector<Address> NodeLocation;

    PeerDiscovery(
            const std::vector<Address>& seeds, Port port = DefaultPort);
    virtual ~PeerDiscovery() { shutdown(); }

    virtual int fd() const { return poller.fd(); }
    virtual void poll(size_t timeoutMs = 0);
    virtual void shutdown();

    virtual WatchHandle discover(const std::string& key, const WatchFn& watch);
    virtual void forget(const std::string& key, WatchHandle handle);
    virtual void lost(const std::string& key, const UUID& keyId);

    virtual void publish(const std::string& key, Payload&& data);
    virtual void retract(const std::string& key);

    void ttl(size_t ttl = DefaultTTL) { ttl_ = ttl; }
    void connExpThresh(size_t ms = DefaultExpThresh) {connExpThresh_ = ms; }

    void period(size_t ms = DefaultPeriod);

    const UUID& id() const { return myId; }
    const NodeLocation& node() const { return myNode; }

private:

    typedef std::string QueryItem;
    typedef std::tuple<std::string, UUID> FetchItem;
    typedef std::tuple<std::string, UUID, Payload> DataItem;
    typedef std::tuple<UUID, NodeLocation, size_t> NodeItem;
    typedef std::tuple<std::string, UUID, NodeLocation, size_t> KeyItem;

    struct ConnState
    {
        int fd;
        size_t id; // sady, fds aren't unique so this is to dedup them.
        UUID nodeId;
        uint32_t version;

        bool isFetch;
        std::vector<FetchItem> pendingFetch;

        ConnState();

        bool initialized() const { return version; }

        void fetch(const std::string& key, const UUID& keyId)
        {
            isFetch = true;
            pendingFetch.emplace_back(key, keyId);
        }
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

        // Used for searching in SortedVectors.
        explicit Item(UUID id) : id(std::move(id)) {}

        Item(KeyItem&& item, double now = lockless::wall()) :
            id(std::move(std::get<1>(item))),
            addrs(std::move(std::get<2>(item))),
            expiration(now * 1000 + std::get<3>(item))
        {}

        Item(NodeItem&& item, double now = lockless::wall()) :
            id(std::move(std::get<0>(item))),
            addrs(std::move(std::get<1>(item))),
            expiration(now * 1000 + std::get<2>(item))
        {}

        Item(UUID id, NodeLocation addrs, size_t ttl, double now = lockless::wall()) :
            id(std::move(id)), addrs(std::move(addrs)), expiration(now * 1000 + ttl)
        {}

        size_t ttl(double now = lockless::wall()) const
        {
            if (expiration <= now * 1000) return 0;
            return expiration - now * 1000;
        }

        void setTTL(size_t ttl, double now = lockless::wall())
        {
            if (ttl > this->ttl(now))
            expiration = now * 1000 + ttl;
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

    struct Fetch
    {
        NodeLocation node;
        size_t delay;

        explicit Fetch(NodeLocation node) :
            node(std::move(node)), delay(1)
        {}
    };

    struct FetchExp
    {
        std::string key;
        UUID keyId;
        double expiration;

        FetchExp(std::string key, UUID keyId, size_t delay, double now = lockless::wall()) :
            key(std::move(key)),
            keyId(std::move(keyId)),
            expiration(now * 1000 + delay)
        {}
    };


    size_t ttl_;
    double period_;
    size_t connExpThresh_;

    UUID myId;
    NodeLocation myNode;

    SortedVector<Item> nodes;
    std::vector<Address> seeds;

    std::unordered_map<int, ConnState> connections;
    std::unordered_map<UUID, int> connectedNodes;
    std::deque<ConnExpItem> connExpiration;
    SortedVector<int> edges;

    std::unordered_map<std::string, std::map<UUID, Fetch> > fetches;
    std::deque<FetchExp> fetchExpiration;

    std::unordered_map<std::string, SortedVector<Item> > keys;
    std::unordered_map<std::string, std::set<Watch> > watches;
    std::unordered_map<std::string, Data> data;

    std::mt19937 rng;

    SourcePoller poller;
    IsPollThread isPollThread;
    Endpoint endpoint;
    Timer timer;


    enum { QueueSize = 1 << 4 };
    Defer<QueueSize, std::string> retracts;
    Defer<QueueSize, std::string, Payload> publishes;
    Defer<QueueSize, std::string, Watch> discovers;
    Defer<QueueSize, std::string, WatchHandle> forgets;
    Defer<QueueSize, std::string, UUID> losts;


    double timerPeriod(size_t ms);
    void discover(const std::string& key, Watch&& watch);
    void onTimer(size_t);
    void onPayload(int fd, const Payload& data);
    void onConnect(int fd);
    void onDisconnect(int fd);

    ConstPackIt onInit (ConnState& conn, ConstPackIt first, ConstPackIt last);
    ConstPackIt onKeys (ConnState& conn, ConstPackIt first, ConstPackIt last);
    ConstPackIt onQuery(ConnState& conn, ConstPackIt first, ConstPackIt last);
    ConstPackIt onNodes(ConnState& conn, ConstPackIt first, ConstPackIt last);
    ConstPackIt onFetch(ConnState& conn, ConstPackIt first, ConstPackIt last);
    ConstPackIt onData (ConnState& conn, ConstPackIt first, ConstPackIt last);

    void sendInitQueries(int fd);
    void sendInitKeys(int fd);
    void sendInitNodes(int fd);
    void sendFetch(const std::string& key, const UUID& keyId, const NodeLocation& node);

    std::pair<bool, UUID> expireItem(SortedVector<Item>& list, double now);
    bool expireKeys(double now);
    void expireFetches(double now);
    void randomDisconnect(double now);
    void randomConnect(double now);
    void seedConnect(double now);

};

} // slick
