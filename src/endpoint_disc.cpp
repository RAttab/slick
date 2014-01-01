/* provider.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Implementation of the provider endpoint
*/

#include "endpoint_disc.h"
#include "pack.h"


/******************************************************************************/
/* ENDPOINT PROVIDER                                                          */
/******************************************************************************/

namespace slick {

EndpointProvider::
~EndpointProvider()
{
    if (!discovery) return;
    discovery->retract(name);
}


void
EndpointProvider::
publish(std::shared_ptr<Discovery> discovery, std::string name)
{
    if (this->discovery)
        this->discovery->retract(this->name);

    this->discovery = std::move(discovery);
    this->name = std::move(name);
    discovery->publish(name, pack(interfaces()));
}


/******************************************************************************/
/* ENDPOINT CLIENT                                                            */
/******************************************************************************/

EndpointClient::
~EndpointClient()
{
    if (!discovery) return;
    discovery->forget(name, handle);
}

void
EndpointClient::
connect(std::shared_ptr<Naming> name, const std::string& endpoint)
{
    if (this->discovery)
        this->discovery->forget(this->name, handle);

    this->discovery = std::move(discovery);
    this->name = std::move(name);

    auto watch = [=] (WatchHandle, const Payload& data) {
        connect(unpack< std::vector<Address> >(data));
    };
    handle = discovery->discover(name, watch);
}

} // slick
