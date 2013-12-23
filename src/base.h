/* base.h                                 -*- C++ -*-
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

#include <vector>
#include <functional>
#include <unordered_map>
#include <cstdint>

namespace slick {

/******************************************************************************/
/* ENDPOINT PROVIDER                                                          */
/******************************************************************************/

typedef int ConnectionHandle;

struct EndpointBase
{
    EndpointBase();
    virtual ~EndpointBase();

    EndpointBase(const EndpointBase&) = delete;
    EndpointBase& operator=(const EndpointBase&) = delete;


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


    struct Stats
    {
        Stats() :
            sendQueueFull(0), sendToUnknown(0), deferPayload(0),
            writableOn(0), writableOff(0)
        {}

        size_t sendQueueFull;
        size_t sendToUnknown;
        size_t deferPayload;

        size_t writableOn;
        size_t writableOff;
    } stats;


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

    void runOperations();
    void deferOperation(Operation&& op);

    bool isOffThread() const;


    size_t pollThread;


    struct ConnectionState
    {
        ConnectionState() : bytesSent(0), bytesRecv(0), writable(false) {}

        Socket socket;

        size_t bytesSent;
        size_t bytesRecv;

        bool writable;
        std::vector<std::pair<Payload, size_t> > sendQueue;
        std::vector<Payload> recvQueue;
    };

    std::unordered_map<ConnectionHandle, ConnectionState> connections;


    struct Operation
    {
        enum Type { Unicast, Broadcast, Connect, Disconnect };

        Operation() {}

        template<typename Payload>
        Operation(Payload&& data) :
            type(Broadcast), data(std::forward<Payload>(data))
        {}

        Operation(ConnectionHandle conn, Payload&& data) :
            type(Unicast), conn(conn), data(std::move(data))
        {}

        Operation(Socket&& socket) :
            type(Connect), connectSocket(std::move(socket))
        {}

        explicit Operation(ConnectionHandle disconnectFd) :
            type(Disconnect), disconnectFd(disconnectFd)
        {}

        Operation(Operation&& other) :
            type(other.type),
            conn(other.conn),
            data(std::move(other.data)),
            connectSocket(std::move(other.connectSocket)),
            disconnectFd(other.disconnectFd)
        {}

        Operation& operator=(Operation&& other)
        {
            type = other.type,

            conn = other.conn;
            data = std::move(other.data);

            connectSocket = std::move(other.connectSocket);
            disconnectFd = other.disconnectFd;

            return *this;
        }

        Operation(const Operation&) = delete;
        Operation& operator=(const Operation&) = delete;

        bool isPayload() const { return type == Unicast || type == Broadcast; }


        Type type;

        ConnectionHandle conn;
        Payload data;

        Socket connectSocket;
        int disconnectFd;
    };

    Queue<Operation, 1U << 6> operations;
    Notify operationsFd;
};


/******************************************************************************/
/* PASSIVE ENDPOINT BASE                                                      */
/******************************************************************************/

struct PassiveEndpointBase : public EndpointBase
{
    PassiveEndpointBase(Port port);
    virtual ~PassiveEndpointBase();

protected:
    virtual void onPollEvent(struct epoll_event& ev);

private:
    PassiveSockets sockets;
};

} // slick
