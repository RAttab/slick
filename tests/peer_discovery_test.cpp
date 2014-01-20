/* peer_discovery_test.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 04 Jan 2014
   FreeBSD-style copyright and disclaimer apply

   Tests for the discovery mechanism.
*/

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include "peer_discovery.h"
#include "discovery_test_utils.h"
#include "test_utils.h"
#include "lockless/tm.h"
#include "lockless/format.h"

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace slick;
using namespace lockless;


template<typename T>
double waitFor(T& value)
{
    double start = lockless::wall();
    while (!value) std::this_thread::yield();
    return lockless::wall() - start;
}

void print(const char* name, const PeerDiscovery& node)
{
    stringstream ss;
    ss << name << ": " << node.id().toString() << " -> [ ";
    for (const auto& addr : node.node()) ss << addr.toString() << " ";
    ss << "]" << endl;
    cerr << ss.str();
}

BOOST_AUTO_TEST_CASE(basics)
{
    cerr << fmtTitle("basics", '=') << endl;

    enum {
        Period = 1,
        WaitPeriod = Period * 2000 + 100,
    };

    const Port Port0 = allocatePort();
    const Port Port1 = allocatePort();


    PeerDiscovery node0({}, Port0);
    node0.period(Period);
    print("node0", node0);

    PollThread poller0;
    poller0.add(node0);
    poller0.run();


    PeerDiscovery node1({ Address("localhost", Port0) }, Port1);
    node1.period(Period);
    print("node1", node0);

    PollThread poller1;
    poller1.add(node1);
    poller1.run();

    // Wait for both nodes to notice each other.
    lockless::sleep(WaitPeriod);

    {
        cerr << fmtTitle("discover-publish", '-') << endl;

        std::atomic<size_t> discovered(0);
        node0.discover("key0", [&] (Discovery::WatchHandle handle, const UUID&, const Payload& data) {
                    discovered = unpack<size_t>(data);
                    printf("node0: key0=%lu\n", discovered.load());
                    node0.forget("key0", handle);
                });

        lockless::sleep(WaitPeriod);
        node1.publish("key0", pack(size_t(1)));

        double elapsed = waitFor(discovered);
        printf("discovery in %s\n\n", fmtElapsed(elapsed).c_str());
        BOOST_CHECK_EQUAL(discovered.load(), 1);
    }

    {
        cerr << fmtTitle("publish-discover", '-') << endl;

        std::atomic<size_t> discovered(0);
        node0.publish("key1", pack(size_t(2)));
        lockless::sleep(WaitPeriod);

        node1.discover("key1", [&] (Discovery::WatchHandle handle, const UUID&, const Payload& data) {
                    discovered = unpack<size_t>(data);
                    printf("node1: key1=%lu\n", discovered.load());
                    node0.forget("key1", handle);
                });

        double elapsed = waitFor(discovered);
        printf("discovery in %s\n\n", fmtElapsed(elapsed).c_str());
        BOOST_CHECK_EQUAL(discovered.load(), 2);
    }

    poller0.join();
    poller1.join();
}

enum SeedPos { None, Front, Back };

std::unique_ptr<PeerDiscovery>
makeNode(PollThread& poller, const NodePool& pool, SeedPos seedPos)
{
    std::vector<Address> seed;
    const auto& nodes = pool.nodes();

    if (seedPos == Front) seed = { nodes.front()->node().front() };
    else if (seedPos == Back) seed = { nodes.back()->node().front() };

    std::unique_ptr<PeerDiscovery> node(new PeerDiscovery(seed, allocatePort()));
    node->period(NodePool::Period);
    node->ttl(NodePool::TTL);
    node->connExpThresh(NodePool::ConnExp);
    poller.add(*node);

    return std::move(node);

}

#if 0 // WIP test

BOOST_AUTO_TEST_CASE(linearPoolTest)
{
    cerr << endl << fmtTitle("linear-pool", '=') << endl;

    NodePool pool(NodePool::Linear, 10);
    PollThread poller;

    auto node0 = makeNode(poller, pool, Front);
    auto node1 = makeNode(poller, pool, Back);

    std::atomic<bool> done(false);

    node0->publish("blah", pack(size_t(1)));
    node1->discover("blah", [&](Discovery::WatchHandle, UUID, const Payload&) {
                printf("\n@@@ GOT IT @@@\n");
                done = true;
            });

    pool.run();
    poller.run();

    for (size_t i = 0; !done && i < 10000; ++i)
        lockless::sleep(1);

    BOOST_CHECK(done);

    poller.join();
    node0->shutdown();
    node1->shutdown();
    pool.shutdown();
}

#endif
