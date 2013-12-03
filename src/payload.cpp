/* payload.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 24 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Payload thingies.
*/

#include "payload.h"
#include "utils.h"

#include <tuple>
#include <memory>
#include <utility>
#include <cassert>
#include <cstdlib>
#include <cstdio>


namespace slick {

namespace proto {


/******************************************************************************/
/* BUFFER                                                                     */
/******************************************************************************/

Payload fromBuffer(const uint8_t* buffer, size_t bufferSize)
{
    size_t size = *reinterpret_cast<const uint16_t*>(buffer);
    if (size > bufferSize) return Payload();
    return Payload(buffer + 2, size);
}


/******************************************************************************/
/* CHUNKED HTTP                                                               */
/******************************************************************************/

#if 0

Payload
Payload::
toChunkedHttp(const Payload& msg)
{
    size_t charSize = (sizeof(msg.size()) - clz(msg.size())) / 4;
    size_t size = charSize + 2 + msg.size() + 2;
    std::unique_ptr<uint8_t> bytes((uint8_t*)std::malloc(size));

    int written =
        snprintf((char*) bytes.get(), size, "%x\r\n%s\r\n",
                unsigned(msg.size()), (char*) msg.bytes());
    assert(size_t(written) == size);

    return Payload(TakeOwnership, bytes.release(), size);
}

namespace {

// Turns out that I can't find a std function to convert a fixed number of char
// into a num without making copies. Oh well.
std::pair<size_t, uint8_t*> readHex(uint8_t* it, uint8_t* last)
{
    size_t size = 0;
    for (; it < last; ++it) {
        size <<= 4;

        if      (*it >= '0' && *it <= '9') size += *it - '0';
        else if (*it >= 'a' && *it <= 'f') size += *it - 'a' + 10;
        else if (*it >= 'A' && *it <= 'F') size += *it - 'A' + 10;
        else return std::make_pair(size, it + 1);
    }

    return std::make_pair(size, last);
}

void testSep(uint8_t* it, uint8_t* last)
{
    assert(last - it >= 2);
    int r = std::memcmp(it, "\r\n", 2);
    assert(!r);
}

} // namespace anonymous

Payload
Payload::
fromChunkedHttp(const Payload& msg)
{
    uint8_t* it = msg.bytes();
    uint8_t* last = it + msg.size();

    size_t size;
    std::tie(size, it) = readHex(it, last);
    assert(it != last);
    assert(it + size + 4 == last);

    testSep(it, last);
    it += 2;

    std::unique_ptr<uint8_t> bytes((uint8_t*)std::malloc(size));
    std::memcpy(bytes.get(), it, size);
    it += size;

    testSep(it, last);

    return Payload(TakeOwnership, bytes.release(), size);
}

#endif

} // namespace proto

} // slick
