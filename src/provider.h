/* provider.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Endpoint provider
*/

#pragma once

#include "socket.h"
#include "base.h"

namespace slick {

/******************************************************************************/
/* ENDPOINT PROVIDER                                                          */
/******************************************************************************/

struct EndpointProvider : public EndpointBase
{
    EndpointProvider(PortRange ports);

    void publish(const std::string& endpoint);

protected:

    virtual void onPollEvent(struct epoll_event& ev);

private:
    PassiveSockets sockets;
};


} // slick
