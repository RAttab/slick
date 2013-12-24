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

    ConnectionHandle connect(const Address& addr);
    void disconnect(ConnectionHandle h);

private:
    std::shared_ptr<Naming> name;
};


/******************************************************************************/
/* CONNECTION                                                                 */
/******************************************************************************/

struct Connection
{
    Connection(EndpointClient& client, const Address& addr) :
        client(client), addr_(addr)
    {
        conn = client.connect(addr_);
    }

    ~Connection() { client.disconnect(conn); }

    const Address& addr() { return addr_; }

private:
    EndpointClient& client;
    ConnectionHandle conn;
    Address addr_;
};

} // slick
