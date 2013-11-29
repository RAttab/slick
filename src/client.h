/* client.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Endpoint client
*/

#include "base.h"

namespace slick {

/******************************************************************************/
/* ENDPOINT CLIENT                                                            */
/******************************************************************************/

struct EndpointClient : public EndpointBase
{
    EndpointClient() {}

    void connect(const std::string& endpoint);
    ConnectionHandle connect(const std::string& host, const std::strign& port)
    {
        connect(ActiveSocket(host.c_str(), port.c_str(), O_NONBLOCK));
    }

    void disconnect(ConnectionHandle h)
    {
        EndpointBase::disconnect(h);
    }
};

} // slick
