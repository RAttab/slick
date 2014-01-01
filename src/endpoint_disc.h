/* endpoint_disc.h                                 -*- C++ -*-
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
/* ENDPOINT PROVIDER                                                          */
/******************************************************************************/

struct EndpointProvider : public PassiveEndpoint
{
    EndpointProvider(Port port)  : PassiveEndpoint(port) {}
    virtual ~EndpointProvider();

    void publish(std::shared_ptr<Discovery> discovery, std::string name);

private:
    std::shared_ptr<Discovery> discovery;
    std::string name;
};


/******************************************************************************/
/* ENDPOINT CLIENT                                                            */
/******************************************************************************/

struct EndpointClient : public Endpoint
{
    EndpointClient() : handle(0) {}
    virtual ~EndpointProvider() {}
    void connect(std::shared_ptr<Discovery> discovery, const std::string& name);

private:
    std::shared_ptr<Discovery> discovery;
    std::string name;
    WatchHandle handle;
};


} // slick
