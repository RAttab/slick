/* endpoint.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 29 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Endpoint base class.
*/

#pragma once

#include "socket.h"
#include "queue.h"
#include "notify.h"
#include "poll.h"
#include "payload.h"
#include "defer.h"

#include <vector>
#include <functional>
#include <unordered_map>
#include <cstdint>

namespace slick {

/******************************************************************************/
/* ENDPOINT PROVIDER                                                          */
/******************************************************************************/

typedef int ConnectionHandle;

struct Endpoint
{
    Endpoint();
    virtual ~Endpoint();

    Endpoint(const Endpoint&) = delete;
    Endpoint& operator=(const Endpoint&) = delete;


    typedef std::function<void(ConnectionHandle h)> ConnectionFn;
    ConnectionFn onNewConnection;
    ConnectionFn onLostConnection;

    typedef std::function<void(ConnectionHandle h, Payload&& d)> PayloadFn;
    PayloadFn onPayload;
    PayloadFn onDroppedPayload;


    int fd() const { return poller.fd(); }
    void poll(int timeoutMs = 0);
    void shutdown();

    void send(ConnectionHandle client, Payload&& data);
    void send(ConnectionHandle client, const Payload& data)
    {
        send(client, Payload(data));
    }

    void broadcast(Payload&& data);
    void broadcast(const Payload& data)
    {
        broadcast(Payload(data));
    }

    // \todo Would be nice to have multicast support.


protected:

    void connect(Socket&& socket);
    void disconnect(int fd);

    virtual void onPollEvent(struct epoll_event&)
    {
        throw std::logic_error("unknown epoll event");
    }

    Epoll poller;

private:

    struct Operation;
    struct ConnectionState;

    void recvPayload(int fd);
    uint8_t* processRecvBuffer(ConnectionState& conn, uint8_t* first, uint8_t* last);

    template<typename Payload>
    void pushToSendQueue(ConnectionState& conn, Payload&& data, size_t offset);

    template<typename Payload>
    bool sendTo(ConnectionState& conn, Payload&& data, size_t offset = 0);

    template<typename Payload>
    void dropPayload(ConnectionHandle h, Payload&& payload) const;

    void flushQueue(int fd);
    void onOperation(Operation&& op);

    IsPollThread isPollThread;

    struct ConnectionState
    {
        ConnectionState() : bytesSent(0), bytesRecv(0), writable(false) {}

        ConnectionState(ConnectionState&&) = default;
        ConnectionState& operator=(ConnectionState&&) = default;

        ConnectionState(const ConnectionState&) = delete;
        ConnectionState& operator=(const ConnectionState&) = delete;

        Socket socket;

        size_t bytesSent;
        size_t bytesRecv;

        bool writable;
        std::vector<std::pair<Payload, size_t> > sendQueue;
        std::vector<Payload> recvQueue;
    };

    std::unordered_map<ConnectionHandle, ConnectionState> connections;

    enum { SendSize = 1 << 6 };
    Defer<SendSize, int, Payload> sends;
    Defer<SendSize, Payload> broadcasts;

    enum { ConnectSize = 1 << 4 };
    Defer<ConnectSize, Socket> connects;
    Defer<ConnectSize, int> disconnects;

    enum { DeferCap = 1 << 6 };
};


/******************************************************************************/
/* PASSIVE ENDPOINT BASE                                                      */
/******************************************************************************/

struct PassiveEndpoint : public Endpoint
{
    PassiveEndpoint(Port port);
    virtual ~PassiveEndpoint();

protected:
    virtual void onPollEvent(struct epoll_event& ev);

private:
    PassiveSockets sockets;
};

} // slick
