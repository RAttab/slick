/* named_endpoint.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Discovery enabled endpoints.
*/

#pragma once

#include "endpoint.h"
#include "discovery.h"

#include <string>
#include <memory>


namespace slick {

/******************************************************************************/
/* NAMED ENDPOINT                                                             */
/******************************************************************************/

struct NamedEndpoint : public Endpoint
{
    NamedEndpoint(Discovery& discovery);
    virtual ~NamedEndpoint();

    int fd() const { return poller.fd(); }
    void poll(int timeoutMs = 0);
    void shutdown();

    void listen(std::string key, Port listenPort, Payload&& data);
    void listen(std::string key, Port listenPort, const Payload& data)
    {
        listen(key, listenPort, Payload(data));
    }

    typedef std::function<bool(const Payload& data)> FilterFn;
    void find(const std::string& key, FilterFn&& filter);
    void find(const std::string& key, const FilterFn& filter)
    {
        find(key, FilterFn(filter));
    }


    // Intentionally hides the callbacks in Endpoint.
    ConnectionFn onLostConnection;

private:

    void onDisconnect(int fd);
    void onWatch(
            const std::string& key,
            Discovery::WatchHandle handle,
            const UUID& keyId,
            const Payload& data);

    Epoll poller;
    IsPollThread isPollThread;

    Endpoint endpoint;
    Discovery& discovery;

    std::string name;

    struct Watch
    {
        std::string key;
        FilterFn filter;

        Watch() {}
        Watch(std::string key, FilterFn&& filter) :
            key(std::move(key)), filter(std::move(filter))
        {}
    };
    std::unordered_map<Discovery::WatchHandle, Watch> activeWatches;

    struct Connection
    {
        std::string key;
        UUID keyId;

        Connection() {}
        Connection(std::string key, UUID keyId) :
            key(std::move(key)), keyId(std::move(keyId))
        {}
    };
    std::unordered_map<int, Connection> connections;

    enum { QueueSize = 1 << 4 };
    Defer<QueueSize, std::string, FilterFn> finds;
    Defer<QueueSize, std::string, Discovery::WatchHandle, UUID, Payload> watches;
};


} // slick
