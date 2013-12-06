/* simple_test.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 30 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Simple tests for the endpoints.
*/

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include "provider.h"
#include "client.h"
#include "utils.h"

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace slick;


BOOST_AUTO_TEST_CASE(basics)
{
    const Port listenPort = 20000;

    enum { Pings = 32 };
    size_t pingRecv = 0, pongRecv = 0;

    SourcePoller poller;

    EndpointProvider provider(listenPort);
    poller.add(provider);

    provider.onNewConnection = [] (ConnectionHandle conn) {
        printf("prv: new %d\n", conn);;
    };
    provider.onLostConnection = [] (ConnectionHandle conn) {
        printf("prv: lost %d\n", conn);;
    };

    provider.onPayload = [&] (ConnectionHandle conn, Payload&& data) {
        std::string msg = proto::toString(data);
        printf("prv: got(%d) %s\n", conn, msg.c_str());
        provider.broadcast(proto::fromString("PONG"));
        pingRecv++;
    };

    EndpointClient client;
    poller.add(client);

    client.onNewConnection = [] (ConnectionHandle conn) {
        printf("cli: new %d\n", conn);;
    };
    client.onLostConnection = [] (ConnectionHandle conn) {
        printf("cli: lost %d\n", conn);;
    };

    client.onPayload = [&] (ConnectionHandle conn, Payload&& data) {
        std::string msg = proto::toString(data);
        printf("cli: got(%d) %s\n", conn, msg.c_str());
        pongRecv++;
    };

    Connection conn(client, "localhost", listenPort);

    std::atomic<bool> shutdown(false);
    auto pollFn = [&] { while (!shutdown) poller.poll(); };
    std::thread pollTh(pollFn);

    for (size_t i = 0; i < Pings; ++i) {
        stringstream ss; ss << "PING { ";
        for (size_t j = 0; j <= i; ++j) ss << to_string(j) << " ";
        ss << "}";

        client.broadcast(proto::fromString(ss.str()));
    }

    slick::sleep(100);
    shutdown = true;
    pollTh.join();

    BOOST_CHECK_EQUAL(Pings, pingRecv);
    BOOST_CHECK_EQUAL(Pings, pongRecv);
}


BOOST_AUTO_TEST_CASE(nice_disconnect)
{
    const Port listenPort = 20001;

    std::atomic<bool> gotClient(false);
    std::atomic<bool> lostClient(false);

    SourcePoller poller;

    EndpointProvider provider(listenPort);
    poller.add(provider);

    provider.onNewConnection = [&] (ConnectionHandle conn) {
        gotClient = true;
        printf("prv: new %d\n", conn);;
    };
    provider.onLostConnection = [&] (ConnectionHandle conn) {
        lostClient = true;
        printf("prv: lost %d\n", conn);;
    };

    EndpointClient client;
    poller.add(client);

    std::atomic<bool> shutdown(false);
    auto pollFn = [&] { while (!shutdown) poller.poll(); };
    std::thread pollTh(pollFn);

    slick::sleep(1);

    auto conn = make_shared<Connection>(client, "localhost", listenPort);
    while (!gotClient);

    conn.reset();
    while(!lostClient);

    shutdown = true;
    pollTh.join();
}

BOOST_AUTO_TEST_CASE(hard_disconnect)
{
    const Port listenPort = 20001;

    Fork fork;
    disableBoostTestSignalHandler();

    if (fork.isParent()) {
        std::atomic<bool> gotClient(false);
        std::atomic<bool> lostClient(false);

        SourcePoller poller;

        EndpointProvider provider(listenPort);
        poller.add(provider);

        provider.onNewConnection = [&] (ConnectionHandle conn) {
            gotClient = true;
            printf("prv: new %d\n", conn);;
        };
        provider.onLostConnection = [&] (ConnectionHandle conn) {
            lostClient = true;
            printf("prv: lost %d\n", conn);;
        };

        std::atomic<bool> shutdown(false);
        auto pollFn = [&] { while (!shutdown) poller.poll(); };
        std::thread pollTh(pollFn);

        while(!gotClient);

        fork.killChild();

        while(!lostClient);

        shutdown = true;
        pollTh.join();
    }

    else {
        SourcePoller poller;

        EndpointClient client;
        poller.add(client);

        std::atomic<bool> shutdown(false);
        auto pollFn = [&] { while (!shutdown) poller.poll(); };
        std::thread pollTh(pollFn);

        Connection conn(client, "localhost", listenPort);

        while(true);
    }
}

