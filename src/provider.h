/* provider.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Endpoint provider
*/

#pragma once

#include "socket.h"

namespace slick {


/******************************************************************************/
/* CLIENT HANDLE                                                              */
/******************************************************************************/

// \todo hide the details in a class.
typedef int ClientHandle;

/******************************************************************************/
/* ENDPOINT PROVIDER                                                          */
/******************************************************************************/

struct EndpointProvider
{

    EndpointProvider(std::string name, const char* port);
    ~EndpointProvider() { shutdown(); }

    int fd() const { return pollFd; }

    void publish(const std::string& endpoint);
    void poll();
    void shutdown();

    std::function<void(ClientHandle h)> onNewClient;
    std::function<void(ClientHandle h)> onLostClient;

    std::function<void(ClientHandle h, Payload&& m)> onPayload;

    void send(ClientHandle client, Payloag&& msg);
    void send(ClientHandle client, const Payload& msg)
    {
        send(client, Payload(msg));
    }

    void broadcast(Payload&& msg);
    void broadcast(const Payload& msg)
    {
        broadcast(Payload(msg));
    }

    // \todo Would be nice to have multicast support.

private:

    void connectClient(int fd);
    void disconnectClient(int fd);

    void recvPayload(int fd);
    void flushQueue(int fd);

    std::string name;
    Epoll poller;
    PassiveSockets sockets;
    size_t pollThread;

    struct ClientState
    {
        ClientState() : bytesSent(0), bytesRecv(0), writable(true) {}

        ActiveSocket socket;

        size_t bytesSent;
        size_t bytesRecv;

        bool writable;
        std::vector<Payload> sendQueue;
    };

    std::unordered_map<ClientHandle, ClientState> clients;
};


} // slick
