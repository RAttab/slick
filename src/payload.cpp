/* payload.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 24 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Payload thingies.
*/

#include "payload.h"

#include <algorithm>

namespace slick {

/******************************************************************************/
/* PAYLOAD                                                                    */
/******************************************************************************/

Payload
Payload::
read(const uint8_t* buffer, size_t bufferSize)
{
    auto size = *reinterpret_cast<const SizeT*>(buffer);
    if (size + sizeof(SizeT) > bufferSize) return Payload();

    size_t totalSize = size + sizeof(SizeT);
    std::unique_ptr<uint8_t[]> bytes(new uint8_t[totalSize]);

    *reinterpret_cast<SizeT*>(bytes.get()) = size;

    const uint8_t* first = buffer + sizeof(SizeT);
    std::copy(first, first + size, bytes.get() + sizeof(SizeT));

    return Payload(bytes.release());
}

void
Payload::
copy(const Payload& other)
{
    SizeT size = other.packetSize();
    const uint8_t* start = other.packet();

    std::unique_ptr<uint8_t[]> bytes(new uint8_t[size]);
    std::copy(start, start + size, bytes.get());

    bytes_ = bytes.release() + 2;
    assert(this->size() == other.size());
}

} // slick
