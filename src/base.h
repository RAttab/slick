/* base.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 29 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Endpoint base class.
*/

#pragma once

namespace slick {

/******************************************************************************/
/* ENDPOINT PROVIDER                                                          */
/******************************************************************************/

typedef int ConnectionHandle;

struct EndpointBase
{
    EndpointBase() : pollThread(0) {}
    virtual ~EndpointBase();

    int fd() const { return pollFd; }

    void poll();
    void shutdown();

    std::function<void(ConnectionHandle h)> onNewConnection;
    std::function<void(ConnectionHandle h)> onLostConnection;

    std::function<void(ConnectionHandle h, Payload&& m)> onPayload;

    void send(ConnectionHandle client, Payloag&& msg);
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

    void connect(ActiveSocket&& socket);
    void disconnect(int fd);

    virtual void onPollEvent(struct epoll_event&)
    {
        throw std::exception("unknown epoll event");
    }

    Epoll poller;

private:

    void recvPayload(int fd);
    void flushQueue(int fd);

    size_t pollThread;

    struct ConnectionState
    {
        ConnectionState() : bytesSent(0), bytesRecv(0), writable(true) {}

        ActiveSocket socket;

        size_t bytesSent;
        size_t bytesRecv;

        bool writable;
        std::vector<Payload> sendQueue;
    };

    std::unordered_map<ConnectionHandle, ConnectionState> connections;
};

} // slick
