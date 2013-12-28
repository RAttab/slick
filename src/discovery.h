/* discovery.h                                  -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 24 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Endpoint discovery.
*/

#pragma once

#include "endpoint.h"
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

    void onTimer();
    void onMessage(ConnectionHandle handle, Payload&& data);
    void onConnect(ConnectionHandle handle);
    void onDisconnect(ConnectionHandle handle);
    void onOperation(Operation&& op);

    SourcePoller poller;
    Timer timer;
    PassiveEndpoint endpoint;

    IsPollThread isPollThread;

    NodeList nodes;

    struct ConnectionState
    {
        ConnectionState() : version(-1) {}
        int version;
    };
    std::unordered_map<ConnectionHandle, ConnectionState> connections;

    std::unordered_map<std::string, NodeList> keyCache;

    typedef std::vector<WatchFn> WatchList;
    std::unordered_map<std::string, WatchList> watches;
    std::unordered_map<std::string, Payload> data;

    struct Operation
    {
        enum Type { None, Discover, Publish, Retract };

        Operation() : type(None) {}

        Operation(std::string key) : type(Retract), key(std::move(key)) {}

        Operation(std::string key, Payload&& data) :
            type(Publish), key(std::move(key))
        {
            pub.data = std::move(data);
        }

        Operation(std::string key, WatchFn watch) :
            type(Discover), key(std::move(key))
        {
            disc.watch = std::move(watch);
        }

        Operation(const Operation&) = delete;
        Operation& operator=(const Operation&) = delete;

        Operation(Operation&&) = default;
        Operation& operator=(Operation&&) = default;


        Type type;

        std::string key;
        struct { WatchFn watch; } disc;
        struct { Payload data; } pub;
    };

    Defer<Operation> operations;

    NodeList::RNG rng;
};

} // slick
