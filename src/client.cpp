/* client.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 30 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Description
*/

#include "client.h"

namespace slick {


/******************************************************************************/
/* ENDPOINT CLIENT                                                            */
/******************************************************************************/

void
EndpointClient::
connect(std::shared_ptr<Naming> name, const std::string& endpoint)
{
    this->name = std::move(name);
    name->discover(endpoint, [=] (Payload&& data) {
                // \todo
            });
}

ConnectionHandle
EndpointClient::
connect(const std::string& host, Port port)
{
    connect(Socket(host.c_str(), port.c_str(), O_NONBLOCK));
}

void
EndpointClient::
disconnect(ConnectionHandle h)
{
    EndpointBase::disconnect(h);
}


} // slick
