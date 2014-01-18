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

struct Endpoint
{
    Endpoint();
    Endpoint(Port listenPort);
    virtual ~Endpoint();

    Endpoint(const Endpoint&) = delete;
    Endpoint& operator=(const Endpoint&) = delete;

    typedef std::function<void(int fd)> ConnectionFn;
    ConnectionFn onNewConnection;
    ConnectionFn onLostConnection;

    typedef std::function<void(int fd, Payload&& d)> PayloadFn;
    PayloadFn onPayload;
    PayloadFn onDroppedPayload;

    typedef std::function<bool(int fd, int errnum)> ErrorFn;
    ErrorFn onError;


    int fd() const { return poller.fd(); }
    void poll(int timeoutMs = 0);
    void shutdown();

    void listen(Port listenPort);

    void send(int fd, Payload&& data);
    void send(int fd, const Payload& data)
    {
        send(fd, Payload(data));
    }

    void broadcast(Payload&& data);
    void broadcast(const Payload& data)
    {
        broadcast(Payload(data));
    }

    // \todo Would be nice to have multicast support.

    void connect(Socket&& socket);
    int connect(const Address& addr);
    int connect(const std::vector<Address>& addrs);

    void disconnect(int fd);


private:

    struct Operation;
    struct ConnectionState;

    void init();

    void accept(int fd);

    void recvPayload(int fd);
    uint8_t* processRecvBuffer(uint8_t*, uint8_t*, std::vector<Payload>&);

    template<typename Payload>
    void pushToSendQueue(ConnectionState& conn, Payload&& data, size_t offset);

    template<typename Payload>
    bool sendTo(ConnectionState& conn, Payload&& data, size_t offset = 0);

    template<typename Payload>
    void dropPayload(int h, Payload&& payload) const;

    void flushQueue(int fd);
    void onOperation(Operation&& op);

    void doDisconnect(std::vector<int> fd);
    void doDisconnect(int fd);


    Epoll poller;
    IsPollThread isPollThread;

    struct ConnectionState
    {
        ConnectionState() :
            bytesSent(0), bytesRecv(0),
            connected(false), disconnected(false), writable(false)
        {}

        ConnectionState(ConnectionState&&) = default;
        ConnectionState& operator=(ConnectionState&&) = default;

        ConnectionState(const ConnectionState&) = delete;
        ConnectionState& operator=(const ConnectionState&) = delete;

        Socket socket;

        size_t bytesSent;
        size_t bytesRecv;

        bool connected;
        bool disconnected;
        bool writable;
        std::vector<std::pair<Payload, size_t> > sendQueue;
    };

    std::unordered_map<int, ConnectionState> connections;

    PassiveSockets listenSockets;

    // Need a seperate queue that can't block when defering from within the
    // polling thread.
    std::vector<int> disconnectQueue;
    Notify disconnectQueueFd;

    enum { SendSize = 1 << 6 };
    Defer<SendSize, int, Payload> sends;
    Defer<SendSize, Payload> broadcasts;

    enum { ConnectSize = 1 << 4 };
    Defer<ConnectSize, Socket> connects;
    Defer<ConnectSize, int> disconnects;

    enum { DeferCap = 1 << 6 };
};


/******************************************************************************/
/* CONNECTION                                                                 */
/******************************************************************************/

struct Connection
{
    Connection(Endpoint& endpoint, const Address& addr) :
        endpoint(endpoint), fd(endpoint.connect(addr))
    {}

    Connection(Endpoint& endpoint, const std::vector<Address>& addrs) :
        endpoint(endpoint), fd(endpoint.connect(addrs))
    {}

    ~Connection() { endpoint.disconnect(fd); }

    operator bool() const { return !fd; }

private:
    Endpoint& endpoint;
    int fd;
};


} // slick
