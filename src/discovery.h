/* discovery.h                                  -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 24 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Endpoint discovery.
*/

#pragma once

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

    virtual void discover(const std::string& key, const WatchFn& watch) = 0;

    virtual void publish(const std::string& key, Payload&& data) = 0;
    void publish(const std::string& key, const Payload& data)
    {
        publish(key, Payload(data));
    }

    virtual void retract(const std::string& key, Payload&& data) = 0;
    void retract(const std::string& key, const Payload& data)
    {
        retract(key, Payload(data));
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

struct DistributedDiscovery
{
    enum { DefaultPort = 18888 };

    DistributedDiscovery(
            const std::vector<Address>& seed, Port port = DefaultPort);

    virtual void fd();
    virtual void poll();

    virtual void discover(const std::string& key, const WatchFn& watch);
    virtual void publish(const std::string& key, Payload&& data);
    virtual void retract(const std::string& key, Payload&& data);

private:

    Epoll poller;
    PassiveEndpointBase endpoint;

    NodeList nodes;
    std::unordered_set<int> connections;

    std::unordered_map<std::string, NodeList> keyCache;

    typedef std::vector<WatchFn> WatchList;
    std::unordered_map<std::string, WatchList> watches;
    std::unordered_map<std::string, Payload> data;

    NodeList::RNG rng;
};

} // slick
