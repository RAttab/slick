/* packet_test.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 06 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Packet sending and timing test.
*/

#include "client.h"
#include "provider.h"
#include "test_utils.h"

#include <vector>
#include <string>
#include <cstdlib>
#include <cassert>

using namespace std;
using namespace slick;


/******************************************************************************/
/* MISC                                                                       */
/******************************************************************************/

enum {
    PayloadSize = 32,
    RefreshRate = 200,
};

string getStats(size_t value, size_t& oldValue)
{
    size_t diff = value - oldValue;
    diff *= 1000 / RefreshRate;

    oldValue = value;

    return fmtValue(diff);
}


/******************************************************************************/
/* PROVIDER                                                                   */
/******************************************************************************/

void runProvider(Port port)
{
    size_t recv = 0, dropped = 0;

    EndpointProvider provider(port);

    provider.onNewConnection = [] (ConnectionHandle conn) {
        fprintf(stderr, "\nprv: new %d\n", conn);;
    };
    provider.onLostConnection = [] (ConnectionHandle conn) {
        fprintf(stderr, "\nprv: lost %d\n", conn);;
    };

    provider.onPayload = [&] (ConnectionHandle conn, Payload&& data) {
        recv++;
        provider.send(conn, move(data));
    };
    provider.onDroppedPayload = [&] (ConnectionHandle, Payload&&) {
        dropped++;
    };

    thread pollTh([&] { while (true) provider.poll(); });

    double start = wall();
    size_t oldRecv = 0;

    while (true) {
        slick::sleep(RefreshRate);

        string diffRecv = getStats(recv, oldRecv);
        string elapsed = fmtElapsed(wall() - start);

        fprintf(stderr, "\r%s> recv: %s ", elapsed.c_str(), diffRecv.c_str());
    }
}


/******************************************************************************/
/* CLIENT                                                                     */
/******************************************************************************/

void runClient(vector<string> uris)
{
    size_t sent = 0, recv = 0, dropped = 0;

    EndpointClient client;

    client.onNewConnection = [] (ConnectionHandle conn) {
        fprintf(stderr, "\ncli: new %d\n", conn);;
    };
    client.onLostConnection = [] (ConnectionHandle conn) {
        fprintf(stderr, "\ncli: lost %d\n", conn);;
    };

    client.onPayload = [&] (ConnectionHandle, Payload&&) {
        recv++;
    };
    client.onDroppedPayload = [&] (ConnectionHandle, Payload&&) {
        dropped++;
    };

    for (auto& uri : uris) client.connect(uri);

    thread pollTh([&] { while (true) client.poll(); });

    Payload payload = proto::fromString(string(PayloadSize, 'a'));
    auto sendFn = [&] {
        while (true) {
            client.broadcast(payload);
            sent++;
        }
    };
    thread sendTh(sendFn);


    double start = wall();
    size_t oldSent = 0, oldRecv = 0;

    while (true) {
        slick::sleep(200);

        string diffSent = getStats(sent - dropped, oldSent);
        string diffRecv = getStats(recv, oldRecv);
        string elapsed = fmtElapsed(wall() - start);

        fprintf(stderr, "\r%s> sent: %s, recv: %s ",
                elapsed.c_str(), diffSent.c_str(), diffRecv.c_str());
    }
}


/******************************************************************************/
/* MAIN                                                                       */
/******************************************************************************/

int main(int argc, char** argv)
{
    assert(argc >= 2);

    if (argv[1][0] == 'p') {
        Port port = 30000;
        if (argc >= 3) port = atoi(argv[2]);
        runProvider(port);
    }

    else if (argv[1][0] == 'c') {
        assert(argc >= 3);

        vector<string> uris;
        for (size_t i = 2; i < size_t(argc); ++i)
            uris.emplace_back(argv[i]);
        runClient(uris);
    }

    else assert(false);

    return 0;
}
