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
/* PORT RANGE                                                                 */
/******************************************************************************/

typedef unsigned Port;
typedef std::pair<Port, Port> PortRange;

/******************************************************************************/
/* ENDPOINT PROVIDER                                                          */
/******************************************************************************/

struct EndpointProvider : public EndpointBase
{
    EndpointProvider(const char* port);

    void publish(const std::string& endpoint);

protected:

    virtual void onPollEvent(struct epoll_event& ev);

private:
    PassiveSockets sockets;
};


} // slick
