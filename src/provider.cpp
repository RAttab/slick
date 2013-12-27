/* provider.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Implementation of the provider endpoint
*/

#include "provider.h"

#include <sys/socket.h>

/******************************************************************************/
/* ENDPOINT PROVIDER                                                          */
/******************************************************************************/

namespace slick {

EndpointProvider::
EndpointProvider(Port port) :
    PassiveEndpoint(port)
{
}


void
EndpointProvider::
publish(std::shared_ptr<Naming> name, const std::string&)
{
    this->name = std::move(name);
    // name->publish(endpoint, ...);
}

} // slick
