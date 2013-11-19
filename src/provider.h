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
    void listen();
    void shutdown();

    std::function<void(const ClientHandle& h)> onNewClient;
    std::function<void(const ClientHandle& h)> onLostClient;

    std::function<void(const ClientHandle& h, Message&& m)> onMessage;

    void send(const ClientHandle& client, Message&& msg);
    void send(const ClientHandle& client, const Message& msg)
    {
        send(client, Message(msg));
    }

    void broadcast(Message&& msg);
    void broadcast(const Message& msg)
    {
        broadcast(Message(msg));
    }

    // \todo Would be nice to have multicast support.

private:

    void connectClient();
    void disconnectClient();
    void processMessage(int fd);

    std::string name;
    int pollFd;
    PassiveSockets sockets;
    std::vector<ClientHandle> clients;
};


} // slick
