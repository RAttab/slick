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
#include "lockless/bits.h"

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
        TTL = 10,
        ConnExp = 5
    };

    typedef std::vector<Address> Node;

    enum Layout { Linear, Central, Random };

    NodePool(Layout layout, size_t n, std::vector<Address> seeds = {}) :
        pollState(Pause)
    {
        for (size_t i = 0; i < n; ++i) {
            nodes_.emplace_back(makeNode(seeds));
            seeds = getSeeds(layout, n);
        }

        pollThread = std::thread([=] { poll(); });
    }

    ~NodePool()
    {
        for (auto node : nodes_) delete node;
    }

    void run() { pollState = Run; }
    void pause() { pollState = Pause; }
    void shutdown()
    {
        pollState = Stop;
        pollThread.join();

        for (auto node : nodes_) node->shutdown();
    }

    const std::vector<PeerDiscovery*> nodes() const { return nodes_; }

private:

    void poll()
    {
        while (pollState != Stop) {
            while (pollState == Pause) lockless::sleep(1);
            poller.poll(1);
        }
    }

    PeerDiscovery* makeNode(const std::vector<Address>& seeds)
    {
        Port port = allocatePort(40000, 50000);

        std::unique_ptr<PeerDiscovery> node(new PeerDiscovery(seeds, port));
        node->period(Period);
        node->ttl(TTL);
        node->connExpThresh(ConnExp);

        poller.add(*node);

        return node.release();
    }

    std::vector<Address> getSeeds(Layout layout, size_t n)
    {
        if (layout == Linear)
            return { nodes_.back()->node().front() };

        if (layout == Central)
            return { nodes_.front()->node().front() };

        size_t picks = lockless::log2(n);
        if (nodes_.size() <= picks)
            return { nodes_.front()->node().front() };

        std::unordered_set<size_t> indexes;
        std::uniform_int_distribution<size_t> dist(0, nodes_.size() -1);
        while (indexes.size() < picks)
            indexes.insert(dist(rng));

        std::vector<Address> seeds;
        for (size_t i : indexes)
            seeds.emplace_back(nodes_[i]->node().front());

        return seeds;
    }


    std::vector<PeerDiscovery*> nodes_;

    enum PollState { Run, Pause, Stop };
    std::atomic<PollState> pollState;
    SourcePoller poller;
    std::thread pollThread;

    std::mt19937 rng;
};


} // slick
