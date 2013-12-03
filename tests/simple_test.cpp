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


BOOST_AUTO_TEST_CASE(simple_test)
{
    const Port listenPort = 20000;

    enum { Pings = 10 };
    size_t pingRecv = 0, pongRecv = 0;

    SourcePoller poller;

    EndpointProvider provider(listenPort);
    poller.add(provider);

    provider.onNewConnection = [] (ConnectionHandle conn) {
        printf("provider: new %d\n", conn);;
    };
    provider.onLostConnection = [] (ConnectionHandle conn) {
        printf("provider: lost %d\n", conn);;
    };

    provider.onPayload = [&] (ConnectionHandle conn, Payload&& data) {
        std::string msg = Payload::toString(data);
        printf("provider: got(%d) %s\n", conn, msg.c_str());
        provider.broadcast(Payload::fromString("PONG"));
        pingRecv++;
    };

    EndpointClient client;
    poller.add(client);

    client.onNewConnection = [] (ConnectionHandle conn) {
        printf("client: new %d\n", conn);;
    };
    client.onLostConnection = [] (ConnectionHandle conn) {
        printf("client: lost %d\n", conn);;
    };

    client.onPayload = [&] (ConnectionHandle conn, Payload&& data) {
        std::string msg = Payload::toString(data);
        printf("client: got(%d) %s\n", conn, msg.c_str());
        pongRecv++;
    };

    Connection conn(client, "localhost", listenPort);

    std::atomic<bool> shutdown(false);
    auto pollFn = [&] { while (!shutdown) poller.poll(); };
    std::thread pollTh(pollFn);

    for (size_t i = 0; i < Pings; ++i) {
        client.broadcast(Payload::fromString(string("PING") + to_string(i)));
        slick::sleep(1);
    }

    slick::sleep(100);
    shutdown = true;
    pollTh.join();

    BOOST_CHECK_EQUAL(Pings, pingRecv);
    BOOST_CHECK_EQUAL(Pings, pongRecv);
}
