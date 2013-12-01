/* provider.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Endpoint provider
*/

#pragma once

#include "base.h"
#include "naming.h"

#include <string>
#include <memory>


namespace slick {

/******************************************************************************/
/* ENDPOINT PROVIDER                                                          */
/******************************************************************************/

struct EndpointProvider : public EndpointBase
{
    EndpointProvider(PortRange ports);
    ~EndpointProvider();

    void publish(std::shared_ptr<Naming> name, const std::string& endpoint);

protected:

    virtual void onPollEvent(struct epoll_event& ev);

private:
    PassiveSockets sockets;
    std::shared_ptr<Naming> name;
};


} // slick
