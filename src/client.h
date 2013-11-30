/* client.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Endpoint client
*/

#include "base.h"
#include "naming.h"

namespace slick {

typedef FdGuard ConnectionGuard;

/******************************************************************************/
/* ENDPOINT CLIENT                                                            */
/******************************************************************************/

struct EndpointClient : public EndpointBase
{
    EndpointClient() {}

    void connect(std::shared_ptr<Naming> name, const std::string& endpoint);

    ConnectionHandle connect(const std::string& host, Port port);
    void disconnect(ConnectionHandle h);

private:
    std::shared_ptr<Naming> name;
};

} // slick
