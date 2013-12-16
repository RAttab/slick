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
    (void) endpoint;
    // name->discover(endpoint, [=] (Payload&& data) {});
}

ConnectionHandle
EndpointClient::
connect(const std::string& host, Port port)
{
    Socket socket(host.c_str(), port, SOCK_NONBLOCK);
    int fd = socket.fd();
    EndpointBase::connect(std::move(socket));
    return fd;
}

ConnectionHandle
EndpointClient::
connect(const std::string& uri)
{
    size_t pos = uri.find(':');
    assert(pos != std::string::npos);

    std::string host = uri.substr(0, pos);
    Port port = stoi(uri.substr(pos + 1));

    return connect(host, port);
}

void
EndpointClient::
disconnect(ConnectionHandle h)
{
    EndpointBase::disconnect(h);
}


} // slick
