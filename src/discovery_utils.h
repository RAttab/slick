/* discovery_utils.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 19 Jan 2014
   FreeBSD-style copyright and disclaimer apply

   Utilities for the discovery implementation.
*/

#pragma once

#include "payload.h"
#include "uuid.h"
#include "address.h"
#include "stream.h"

#include <sstream>
#include <iostream>

namespace slick {

/******************************************************************************/
/* DEBUG                                                                      */
/******************************************************************************/

namespace {

std::ostream& operator<<(std::ostream& stream, const Payload& data)
{
    stream << "<pl:" << data.size() << ">";
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const UUID& uuid)
{
    // stream << uuid.toString();
    stream << lockless::format("%08x", uuid.time_low);
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const Address& addr)
{
    stream << addr.toString();
    return stream;
}


template<typename... Args>
void print(UUID& id, const char* action, const Args&... args)
{
    std::stringstream ss;
    ss << id << ": " << action << "(";
    streamAll(ss, args...);
    ss << ")\n";
    std::cerr << ss.str();
}

} // namespace anonymous



} // slick
