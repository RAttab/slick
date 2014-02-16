/* discovery.h                                  -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 24 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Endpoint discovery.

   \todo Fix the bajillion races with watch removal.
*/

#pragma once

#include "payload.h"
#include "uuid.h"

#include <string>
#include <functional>

namespace slick {

/******************************************************************************/
/* DISCOVERY                                                                  */
/******************************************************************************/

struct Discovery
{
    typedef size_t WatchHandle;
    typedef std::function<void(WatchHandle, const UUID&, const Payload&)> WatchFn;

    virtual WatchHandle discover(const std::string& key, const WatchFn& watch) = 0;
    virtual void forget(const std::string& key, WatchHandle handle) = 0;
    virtual void lost(const std::string& key, const UUID& keyId) = 0;

    virtual void retract(const std::string& key) = 0;
    virtual void publish(const std::string& key, Payload&& data) = 0;
    void publish(const std::string& key, const Payload& data)
    {
        publish(key, Payload(data));
    }

};

} // slick
