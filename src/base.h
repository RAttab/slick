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

    int fd() const { return poller.fd(); }

    void poll();
    void shutdown();

    typedef std::function<void(ConnectionHandle h)> ConnectionFn;
    ConnectionFn onNewConnection;
    ConnectionFn onLostConnection;

    std::function<void(ConnectionHandle h, Payload&& d)> onPayload;

    void send(ConnectionHandle client, Payload&& msg);
    void send(ConnectionHandle client, const Payload& msg)
    {
        send(client, Payload(msg));
    }

    void broadcast(Payload&& msg);
    void broadcast(const Payload& msg)
    {
        broadcast(Payload(msg));
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

    void recvPayload(int fd);
    void flushQueue(int fd);
    void flushMessages();
    uint8_t* processRecvBuffer(int fd, uint8_t* first, uint8_t* last);


    size_t pollThread;

    struct ConnectionState
    {
        ConnectionState() : bytesSent(0), bytesRecv(0), writable(false) {}

        Socket socket;

        size_t bytesSent;
        size_t bytesRecv;

        bool writable;
        std::vector<Payload> sendQueue;
    };

    std::unordered_map<ConnectionHandle, ConnectionState> connections;


    struct Message
    {
        enum Type { Unicast, Broadcast, Connect, Disconnect };

        Message() : conn(-1) {}

        template<typename Payload>
        Message(Payload&& data) :
            type(Broadcast), data(std::forward<Payload>(data))
        {}

        Message(ConnectionHandle conn, Payload&& data) :
            type(Unicast), conn(conn), data(std::move(data))
        {}

        Message(Socket&& socket) :
            type(Connect), connectSocket(std::move(socket))
        {}

        explicit Message(ConnectionHandle disconnectFd) :
            type(Disconnect), disconnectFd(disconnectFd)
        {}

        Message(Message&& other) :
            type(other.type),
            conn(other.conn),
            data(std::move(other.data)),
            connectSocket(std::move(other.connectSocket)),
            disconnectFd(other.disconnectFd)
        {}

        Message& operator=(Message&& other)
        {
            type = other.type,

            conn = other.conn;
            data = std::move(other.data);

            connectSocket = std::move(other.connectSocket);
            disconnectFd = other.disconnectFd;

            return *this;
        }

        Message(const Message&) = delete;
        Message& operator=(const Message&) = delete;

        Type type;

        ConnectionHandle conn;
        Payload data;

        Socket connectSocket;
        int disconnectFd;
    };

    Queue<Message, 1U << 6> messages;
    Notify messagesFd;
};

} // slick
