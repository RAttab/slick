/* client.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Endpoint client
*/

#include "base.h"
#include "naming.h"

#include <memory>
#include <string>

namespace slick {

/******************************************************************************/
/* ENDPOINT CLIENT                                                            */
/******************************************************************************/

struct EndpointClient : public EndpointBase
{
    EndpointClient() {}

    void connect(std::shared_ptr<Naming> name, const std::string& endpoint);

    ConnectionHandle connect(const std::string& host, Port port);
    ConnectionHandle connect(const std::string& uri);
    void disconnect(ConnectionHandle h);

private:
    std::shared_ptr<Naming> name;
};


/******************************************************************************/
/* CONNECTION                                                                 */
/******************************************************************************/

struct Connection
{
    Connection(EndpointClient& client, const std::string& host, Port port) :
        client(client), host_(host), port_(port)
    {
        conn = client.connect(host, port);
    }

    ~Connection() { client.disconnect(conn); }

    const std::string& host() const { return host_; }
    Port port() const { return port_; }

private:
    EndpointClient& client;
    ConnectionHandle conn;
    std::string host_;
    Port port_;
};

} // slick
