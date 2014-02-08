/* peers.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 19 Jan 2014
   FreeBSD-style copyright and disclaimer apply

   Peer management utility.
*/

#pragma once

#include "timer.h"
#include "address.h"
#include "sorted_vector.h"

#include <queue>
#include <unordered_map>
#include <functional>

namespace slick {

struct Endpoint;

/******************************************************************************/
/* CONNECTIONS                                                                */
/******************************************************************************/

struct NoData {};
typedef size_t PeerId;
enum class PeerModel { Persistent, Rotate };

template<typename ConnectionData = NoData>
struct Peers
{
    Peers(PeerModel model, Endpoint& endpoint, double period);
    ~Peers();

    void period(double value);

    typedef std::function<void(PeerId id)> ConnectFn;
    ConnectFn onConnect;
    ConnectFn onDisconnect;
    void notifyConnect(int fd);
    void notifyDisconnect(int fd);


    size_t add(NodeAddress peer);
    size_t add(int fd, NodeAddress peer);
    void remove(PeerId id);
    bool test(int fd) const;

    size_t peers() const;
    size_t connections() const;

    bool connected(PeerId id) const;
    const NodeAddress& addr(size_t PeerId) const;
    ConnectionData& data(PeerId id);
    const ConnectionData& data(PeerId id) const;


    template<typename Payload>
    void send(PeerId id, Payload&& data);

    template<typename Payload>
    void broadcast(Payload&& data);


    int fd() const { poller.fd; }
    void poll(size_t timeoutMs = 0) { poller.poll(timeoutMs); }
    void shutdown();

private:

    struct Peer
    {
        PeerId id;
        int fd;
        NodeAddress addr;
        size_t lastWaitMs;

        Peer(PeerId id = 0, NodeAddress addr = {}) :
            id(id), fd(-1), addr(std::move(addr)), lastWaitMs(0)
        {}
    };

    struct Connection
    {
        int fd;
        PeerId id;
        ConnectionData data;

        Connection(int fd = -1, PeerId id = 0) :
            fd(fd), id(id)
        {}

        bool connected() const { return fd > 0; }
    };


    Model model;
    double period_;

    PollSource poller;

    Endpoint& endpoint;
    Timer timer;

    PeerId idCounter;
    std::unordered_map<size_t, Peer> peers;

    std::unordered_map<int, Connection> connections;
    SortedVector<int> broadcastFds;

    std::mt19937 rng;
    TimeoutQueue<PeerId> deadlines;

    double calcPeriod(double value);

    Peer& peer(PeerId id);
    const Peer& peer(PeerId id) const;
    Connection& connection(PeerId id);
    const Connection& connection(PeerId id) const;

    void connectPeer(Peer& peer);

    void onTimer(uint64_t);
    void onTimeout(PeerId);
    void reconnect(const Deadline& deadline);
    void topupConnections();
};

} // slick
