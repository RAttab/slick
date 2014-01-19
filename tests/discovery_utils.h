/* discovery_utils.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 19 Jan 2014
   FreeBSD-style copyright and disclaimer apply

   Discovery testing utils.
*/

#pragma once

#include "peer_discovery.h"
#include "socket.h"
#include "poll.h"
#include "test_utils.h"

#include <atomic>
#include <thread>
#include <vector>

namespace slick {


/******************************************************************************/
/* NODE POOL                                                                  */
/******************************************************************************/

struct NodePool
{
    enum {
        Period = 1,
        TTL = 4,
        ConnExp = 2
    };

    typedef std::vector<Address> Node;

    enum Layout { Linear, Central, Random };

    NodePool(Layout layout, size_t n, std::vector<Node> seeds = {}) :
        pollState(Pause)
    {
        for (size_t i = 0; i < n; ++i) {
            nodes.emplace_back(makeNode(seed));
            seeds = getSeeds(layout, n);
        }

        pollThread = std::thread([=] { poll(); });
    }

    ~NodePool()
    {
        for (auto node : nodes) delete node;
    }

    void run() { pollState = Run; }
    void pause() { pollState = Pause; }
    void shutdown()
    {
        pollState = Shutdown;
        pollThread.join();

        for (auto node : nodes) node->shutdown();
    }

    const std::vector<PeerDiscovery*> nodes() const { return nodes; }

private:

    void poll()
    {
        while (pollState != Shutdown) {
            while (pollState == Pause) lockless::sleep(1);
            poller.poll(1);
        }
    }

    PeerDiscovery* makeNode(const std::vector<Node>& seeds)
    {
        Port port = allocatePort(40000, 50000);

        std::unique_ptr<PeerDiscovery> node(new PeerDiscovery(seeds, port));
        node->period(Period);
        node->ttl(TTL);
        node->connExpThresh(ConnExp);

        poller.add(node->fd());

        return std::move(node);
    }

    std::vector<Node> getSeeds(Layout layout, size_t n)
    {
        if (layout == Linear)
            return {{ nodes.back().node() }};

        if (layout == Central && seeds.empty())
            return {{ nodes.front().node() }};

        size_t picks = lockless::log2(n);
        if (nodes.size() <= picks)
            return {{ nodes.front().node() }};

        std::unordered_set<size_t> indexes;
        std::uniform_int_distribution<size_t> dist(0, nodes.size() -1);
        while (indexes.size() < picks)
            indexes.insert(dist(rng));

        std::vector<Node> seeds;
        for (size_t i : indexes)
            seeds.emplace_back(nodes[i].node());

        return seeds;
    }


    std::vector<PeerDiscovery*> nodes;

    enum PollState { Run, Pause, Stop };
    std::atomic<PollState> pollState;
    SourcePoller poller;
    std::thread pollThread;

    std::mt19937 rng;
};


} // slick
