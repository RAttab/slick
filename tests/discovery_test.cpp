/* discovery_test.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 04 Jan 2014
   FreeBSD-style copyright and disclaimer apply

   Tests for the discovery mechanism.
*/

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include "discovery.h"
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
    while (!value);
    return lockless::wall() - start;
}

const char* fmt(const std::vector<Address>& node)
{
    stringstream ss; ss << "[ ";
    for (const auto& addr : node) ss << addr.toString() << " ";
    ss << "]";
    return ss.str().c_str();
}

const char* fmt(const UUID& uuid)
{
    return uuid.toString().c_str();
}

BOOST_AUTO_TEST_CASE(basics)
{
    cerr << fmtTitle("basics", '=') << endl;

    enum {
        Period = 1,
        WaitPeriod = Period * 2000 + 100,
    };

    // Need some random on the ports otherwise you get periodic weird failures
    // when the test crash and isn't able to properly close the connection. I
    // believe the kernel lags the ipv4 closing while immediately cleaning up
    // the ipv6. This means that we're able to bind to the ipv6 but we only ever
    // try to connect to the ipv4 (luck of the interface sorting).
    const Port Port0 = 1888 + (lockless::rdtsc() % 100);
    const Port Port1 = Port0 + 1;


    DistributedDiscovery node0({}, Port0);
    node0.setPeriod(Period);
    printf("node0: %s -> %s\n", fmt(node0.id()), fmt(node0.node()));

    PollThread poller0;
    poller0.add(node0);
    poller0.run();


    DistributedDiscovery node1({ Address("localhost", Port0) }, Port1);
    node1.setPeriod(Period);
    printf("node1: %s -> %s\n", fmt(node1.id()), fmt(node1.node()));

    PollThread poller1;
    poller1.add(node1);
    poller1.run();

    // Wait for both nodes to notice each other.
    lockless::sleep(WaitPeriod);

    {
        cerr << fmtTitle("discover-publish", '-') << endl;

        std::atomic<size_t> discovered(0);
        node0.discover("key0", [&] (Discovery::WatchHandle handle, const Payload& data) {
                    discovered = unpack<size_t>(data);
                    printf("node0: key0=%lu\n", discovered.load());
                    node0.forget("key0", handle);
                });

        lockless::sleep(WaitPeriod);
        node1.publish("key0", pack(size_t(1)));

        double elapsed = waitFor(discovered);
        printf("\ndiscovery in %s\n", fmtElapsed(elapsed).c_str());
        BOOST_CHECK_EQUAL(discovered.load(), 1);
    }

    {
        cerr << fmtTitle("publish-discover", '-') << endl;

        std::atomic<size_t> discovered(0);
        node0.publish("key1", pack(size_t(2)));
        lockless::sleep(WaitPeriod);

        node1.discover("key1", [&] (Discovery::WatchHandle handle, const Payload& data) {
                    discovered = unpack<size_t>(data);
                    printf("node1: key1=%lu\n", discovered.load());
                    node0.forget("key1", handle);
                });

        double elapsed = waitFor(discovered);
        printf("\ndiscovery in %s\n", fmtElapsed(elapsed).c_str());
        BOOST_CHECK_EQUAL(discovered.load(), 2);
    }

    poller0.join();
    poller1.join();
}
