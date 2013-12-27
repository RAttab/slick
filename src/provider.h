/* provider.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Endpoint provider
*/

#pragma once

#include "endpoint.h"
#include "naming.h"

#include <string>
#include <memory>


namespace slick {

/******************************************************************************/
/* ENDPOINT PROVIDER                                                          */
/******************************************************************************/

struct EndpointProvider : public PassiveEndpoint
{
    EndpointProvider(Port port);
    virtual ~EndpointProvider() {}

    void publish(std::shared_ptr<Naming> name, const std::string& endpoint);

private:
    std::shared_ptr<Naming> name;
};


} // slick
