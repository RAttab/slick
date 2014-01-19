/* static_discovery.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 19 Jan 2014
   FreeBSD-style copyright and disclaimer apply

   Discovery network where each node knows and is connected to every other nodes
   in the network. The protocol is less chatty but doesn't scale as well as
   PeerDiscovery.
*/

#pragma once

#include "discovery.h"
#include "endpoint.h"
#include "pack.h"
#include "poll.h"
#include "defer.h"

namespace slick {


/******************************************************************************/
/* STATIC DISCOVERY                                                           */
/******************************************************************************/

struct StaticDiscovery
{
    enum {
        DefaultPort = 19999,
        DefaultPeriod = 60,
    };

    StaticDiscovery(std::vector<Address> peers, Port port = DefaultPort);

    virtual int fd() const { return poller.fd(); }
    virtual void poll(size_t timeoutMs = 0);
    virtual void shutdown();

    virtual WatchHandle discover(const std::string& key, const WatchFn& watch);
    virtual void forget(const std::string& key, WatchHandle handle);
    virtual void lost(const std::string& key, const UUID& keyId);

    virtual void retract(const std::string& key);
    virtual void publish(const std::string& key, Payload&& data);

    void period(size_t sec = DefaultPeriod) const;

private:

    struct Conn
    {
        int fd;
        size_t peerIndex;
        uint32_t version;

        Conn(int fd, size_t peerIndex) :
            fd(fd), peerIndex(peerIndex)
        {}

        bool initialized() const { return version; }
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

    struct Data
    {
        UUID id;
        Payload data;

        Data() {}
        explicit Data(Payload data) :
            id(UUID::random()), data(std::move(data))
        {}
    };

    UUID myId;

    size_t period_;

    SourcePoller poller;
    IsPollThread isPollThread;
    Endpoint endpoint;
    Timer timer;

    const std::vector<Address> peers;

    std::unordered_map<int, Conn> connections;
    std::unordered_set<size_t> disconnectedPeers;
    SortedVector<int> edges;

    std::unordered_map<std::string, Data> data;
    std::unordered_map<std::string, std::set<Watch> > watches;
    std::unordered_map<std::string, std::unordered_map<UUID, Payload> > keys;


    enum { QueueSize = 1 << 4 };
    Defer<QueueSize, std::string> retracts;
    Defer<QueueSize, std::string, Payload> publishes;
    Defer<QueueSize, std::string, Watch> discovers;
    Defer<QueueSize, std::string, WatchHandle> forgets;
    Defer<QueueSize, std::string, UUID> losts;

    size_t timerPeriod(size_t secs);
    void discover(const std::string& key, Watch&& watch);
    void onTimer(size_t);
    void onPayload(int fd, const Payload& data);
    void onConnect(int fd);
    void onDisconnect(int fd);

    ConstPackIt onInit (ConnState& conn, ConstPackIt first, ConstPackIt last);
    ConstPackIt onQuery(ConnState& conn, ConstPackIt first, ConstPackIt last);
    ConstPackIt onKeys (ConnState& conn, ConstPackIt first, ConstPackIt last);
};


} // slick
