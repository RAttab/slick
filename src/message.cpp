/* message.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 24 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Message thingies.
*/

#include "message.h"
#include "utils.h"

#include <tuple>
#include <memory>
#include <cassert>
#include <cstdlib>


namespace slick {


/******************************************************************************/
/* MESSAGE                                                                    */
/******************************************************************************/

Message(const uint8_t* src, size_t size) :
    size_(size), bytes_(std::malloc(size))
{
    std::memcpy(src, bytes_, size);
}


/******************************************************************************/
/* CHUNKED HTTP                                                               */
/******************************************************************************/

Message&& toChunkedHttp(const Message& msg)
{
    size_t charSize = (sizeof(msg.size()) - clz(msg.bytes())) / 4;
    size_t size = charSize + 2 + msg.size() + 2;
    std::unique_ptr<uint8_t> bytes(std::malloc(size));

    int written =
        snprintf(bytes.get(), size, "%x\r\n%s\r\n", msg.size(), msg.bytes());
    std::assert(written == size);

    return Message(TakeOwnership, bytes.release(), size);
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
    std::assert(last - it >= 2);
    int r = std::memcmp(it, "\r\n", 2);
    std::assert(!r);
}

} // namespace anonymous

Message&& fromChunkedHttp(const Message& msg)
{
    uint8_t* it = msg.bytes(), last = first + msg.size();

    size_t size;
    std::tie(size, it) = readHex(it, last);
    std::assert(it != last);
    std::assert(it + size + 4 == last);

    testSep(it, last);
    it += 2;

    std::unique_ptr<uint8_t> bytes(std::malloc(size));
    std::memcpy(bytes.get(), it, size);
    it += size;

    testSep(it, last);

    return Message(TakeOwnership, bytes.release(), size);
}

} // slick
