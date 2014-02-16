/* endpoint_test.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 30 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Simple tests for the endpoints.
*/

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include "endpoint.h"
#include "pack.h"
#include "utils.h"
#include "test_utils.h"
#include "lockless/format.h"
#include "lockless/tm.h"

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace slick;
using namespace lockless;

namespace { Port portCounter = 20000; }

BOOST_AUTO_TEST_CASE(interfaces_lp)
{
    cerr << fmtTitle("interfaces-lp", '=') << endl;

    auto interfaces = networkInterfaces();

    for (size_t i = 0; i < interfaces.size(); ++i)
        printf("%lu: %s\n", i, interfaces[i].toString().c_str());

}

BOOST_AUTO_TEST_CASE(interfaces_no_lp)
{
    cerr << fmtTitle("interfaces-no-lp", '=') << endl;

    auto interfaces = networkInterfaces(true);
    for (size_t i = 0; i < interfaces.size(); ++i)
        printf("%lu: %s\n", i, interfaces[i].toString().c_str());

}


BOOST_AUTO_TEST_CASE(basics)
{
    cerr << fmtTitle("basics", '=') << endl;

    const Port listenPort = portCounter++;

    enum { Pings = 32 };
    size_t pingRecv = 0, pongRecv = 0;

    PollThread poller;

    Endpoint provider(listenPort);
    poller.add(provider);

    provider.onNewConnection = [] (int fd) {
        printf("prv: new %d\n", fd);
    };
    provider.onLostConnection = [] (int fd) {
        printf("prv: lost %d\n", fd);
    };

    provider.onPayload = [&] (int fd, Payload&& data) {
        auto msg = unpack<std::string>(data);
        printf("prv: got(%d) %s\n", fd, msg.c_str());
        provider.broadcast(pack("PONG"));
        pingRecv++;
    };

    Endpoint client;
    poller.add(client);

    client.onNewConnection = [] (int fd) {
        printf("cli: new %d\n", fd);
    };
    client.onLostConnection = [] (int fd) {
        printf("cli: lost %d\n", fd);
    };

    client.onPayload = [&] (int fd, Payload&& data) {
        auto msg = unpack<std::string>(data);
        printf("cli: got(%d) %s\n", fd, msg.c_str());
        pongRecv++;
    };

    Connection conn(client, { "localhost", listenPort });

    poller.run();

    for (size_t i = 0; i < Pings; ++i) {
        stringstream ss; ss << "PING { ";
        for (size_t j = 0; j <= i; ++j) ss << to_string(j) << " ";
        ss << "}";

        client.broadcast(pack(ss.str()));
    }

    lockless::sleep(100);
    poller.join();

    BOOST_CHECK_EQUAL(Pings, pingRecv);
    BOOST_CHECK_EQUAL(Pings, pongRecv);
}

BOOST_AUTO_TEST_CASE(n_to_n)
{
    cerr << fmtTitle("n_to_n", '=') << endl;

    enum { N = 100 };


    // PROVIDERS ---------------------------------------------------------------
    cerr << fmtTitle("providers") << endl;

    const Port listenPortStart = 30000;

    PollThread provPoller;

    array<shared_ptr<Endpoint>, N> providers;
    array<size_t, N> clientIdSums;
    clientIdSums.fill(0);

    for (size_t id = 0; id < N; ++id) {
        providers[id] = make_shared<Endpoint>(listenPortStart + id);
        provPoller.add(*providers[id]);

        weak_ptr<Endpoint> prov(providers[id]);
        providers[id]->onPayload = [=, &clientIdSums] (int fd, Payload&& data) {
            clientIdSums[id] += unpack<size_t>(data);

            auto ptr = prov.lock();
            ptr->send(fd, pack<size_t>(id + 1));
        };

        providers[id]->onDroppedPayload = [] (int, Payload&&) {
            assert(false);
        };

    }

    provPoller.run();

    // CLIENTS -----------------------------------------------------------------
    cerr << fmtTitle("clients") << endl;

    PollThread clientPoller;

    Endpoint client;
    clientPoller.add(client);

    client.onDroppedPayload = [] (int, Payload&&) {
        assert(false);
    };

    std::atomic<size_t> provIdSum(0);
    client.onPayload = [&] (int, Payload&& data) {
        provIdSum += unpack<size_t>(data);
    };

    array<shared_ptr<Connection>, N> connections;

    clientPoller.run();

    for (size_t id = 0; id < N; ++id) {
        connections[id] = make_shared<Connection>(
                client, Address("localhost", listenPortStart + id));
    }


    // TEST --------------------------------------------------------------------
    cerr << fmtTitle("test") << endl;

    client.broadcast(pack<size_t>(1));

    size_t exp = (N * (N + 1)) / 2;
    while (provIdSum != exp);

    cerr << fmtTitle("done") << endl;

    provPoller.join();
    clientPoller.join();

    for (size_t i = 0; i < N; ++i)
        BOOST_CHECK_EQUAL(clientIdSums[i], 1);
}


BOOST_AUTO_TEST_CASE(nice_disconnect)
{
    cerr << fmtTitle("nice_disconnecct", '=') << endl;

    const Port listenPort = portCounter++;

    std::atomic<bool> gotClient(false);
    std::atomic<bool> lostClient(false);

    PollThread poller;

    Endpoint provider(listenPort);
    poller.add(provider);

    provider.onNewConnection = [&] (int fd) {
        gotClient = true;
        printf("prv: new %d\n", fd);
    };
    provider.onLostConnection = [&] (int fd) {
        lostClient = true;
        printf("prv: lost %d\n", fd);
    };

    Endpoint client;
    poller.add(client);

    poller.run();

    lockless::sleep(1);

    auto conn = make_shared<Connection>(client, Address("localhost", listenPort));
    while (!gotClient);

    conn.reset();
    while(!lostClient);

    poller.join();
}

BOOST_AUTO_TEST_CASE(hard_disconnect)
{
    cerr << fmtTitle("hard_disconnecct", '=') << endl;

    const Port listenPort = portCounter++;

    Fork fork;
    disableBoostTestSignalHandler();

    if (fork.isParent()) {
        std::atomic<bool> gotClient(false);
        std::atomic<bool> lostClient(false);

        PollThread poller;

        Endpoint provider(listenPort);
        poller.add(provider);

        provider.onNewConnection = [&] (int fd) {
            gotClient = true;
            printf("prv: new %d\n", fd);
        };
        provider.onLostConnection = [&] (int fd) {
            lostClient = true;
            printf("prv: lost %d\n", fd);;
        };

        poller.run();

        while(!gotClient);

        fork.killChild();

        while(!lostClient);

        poller.join();
    }

    else {
        PollThread poller;

        Endpoint client;
        poller.add(client);

        poller.run();

        Connection conn(client, { "localhost", listenPort });

        while(true);
    }
}
