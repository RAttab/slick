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


    struct Operation
    {
        enum Type { None, Unicast, Broadcast, Connect, Disconnect };

        Operation() : type(None) {}

        Operation(int fd, Payload&& data) : type(Unicast)
        {
            send.fd = fd;
            send.data = std::move(data);
        }

        template<typename Payload>
        Operation(Payload&& data) : type(Broadcast)
        {
            send.data = std::forward<Payload>(data);
        }

        Operation(Socket&& socket) : type(Connect)
        {
            connect.socket = std::move(socket);
        }

        explicit Operation(int disconnectFd) : type(Disconnect)
        {
            disconnect.fd = disconnectFd;
        }

        Operation(Operation&& other) noexcept
        {
            *this = std::move(other);
        }

        Operation& operator=(Operation&& other) noexcept
        {
            type = other.type;
            send.fd = other.send.fd;
            send.data = std::move(other.send.data);
            connect.socket = std::move(other.connect.socket);
            disconnect.fd = other.disconnect.fd;

            return *this;
        }

        Operation(const Operation&) = delete;
        Operation& operator=(const Operation&) = delete;

        bool isPayload() const { return type == Unicast || type == Broadcast; }

        Type type;

        struct {
            int fd;
            Payload data;
        } send;

        struct { Socket socket; } connect;
        struct { int fd; } disconnect;
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
