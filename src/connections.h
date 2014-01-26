/* connections.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 19 Jan 2014
   FreeBSD-style copyright and disclaimer apply

   Connection manager utilities.
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

template<typename Data>
struct Connections
{
    enum Model { Persistent, Rotate };

    Connections(Model model, Endpoint& endpoint, double period);
    ~Connections();

    void period(double value);

    typedef std::function<void(size_t peerId)> ConnectFn;
    ConnectFn onConnect;
    ConnectFn onDisconnect;
    void notifyConnect(int fd);
    void notifyDisconnect(int fd);


    size_t add(NodeAddress peer);
    size_t add(int fd, NodeAddress peer);
    void remove(size_t peerId);
    bool test(int fd) const;

    size_t peers() const;
    size_t connections() const;

    bool connected(size_t peerId) const;
    const NodeAddress& addr(size_t PeerId) const;
    Data& data(size_t peerId);
    const Data& data(size_t peerId) const;


    template<typename Payload>
    void send(size_t peerId, Payload&& data);

    template<typename Payload>
    void broadcast(Payload&& data);


    int fd() const { timer.fd; }
    void poll(size_t timeoutMs = 0);
    void shutdown();

private:

    struct Peer
    {
        size_t peerId;
        int fd;
        NodeAddress addr;
        size_t lastWaitMs;

        Peer(size_t peerId = 0, NodeAddress addr = {}) :
            peerId(peerId), fd(-1), addr(std::move(addr)), lastWaitMs(0)
        {}
    };

    struct Connection
    {
        int fd;
        size_t peerId;
        Data data;

        Connection(int fd = -1, size_t peerId = 0) :
            fd(fd), peerId(peerId)
        {}

        bool connected() const { return fd > 0; }
    };

    struct Deadline
    {
        size_t peerId;
        double deadline;

        Deadline() : peerId(0) {}
        Deadline(size_t peerId, size_t waitMs, double now = lockless::now()) :
            peerId(peerId), deadline(now + double(waitMs) / 1000)
        {}

        bool operator<(const Deadline& other) const
        {
            return deadline < other.deadline;
        }
    };

    Model model;
    double period_;

    Endpoint& endpoint;
    Timer timer;

    size_t peerIdCounter;
    std::unordered_map<size_t, Peer> peers;

    std::unordered_map<int, Connection> connections;
    SortedVector<int> broadcastFds;

    std::mt19937 rng;
    std::priority_queue<Deadline> deadlines;

    double calcPeriod(double value);

    Peer& peer(size_t peerId);
    const Peer& peer(size_t peerId) const;
    Connection& connection(size_t peerId);
    const Connection& connection(size_t peerId) const;

    void connectPeer(Peer& peer);

    void onTimer(uint64_t);
    void reconnect(const Deadline& deadline);
    void topupConnections(double now);
};

} // slick
