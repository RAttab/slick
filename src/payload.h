/* payload.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Payload seraizlization utilities.

   Might want to consider just using shared_ptr to make everything much simpler.
*/

#pragma once

#include <memory>
#include <string>
#include <functional>
#include <algorithm>
#include <cstdint>
#include <cassert>
#include <cstring>

namespace slick {


/******************************************************************************/
/* MESSAGE                                                                    */
/******************************************************************************/

/** Data layout looks like this:

    +-------+--------------+
    | SizeT | ... data ... |
    +-------+--------------+
    |       |
    packet  bytes

    We store things this way because when we transmit on the wire we'll need to
    know the size of the packet to properly reconstruct it. Keeping the size
    with the bytes means that the packet is already properly formatted for
    sending over the wire which means that we avoid a copy.

    This also explains the distinction between the packet() and bytes()
    functions.
 */
struct Payload
{
    typedef uint16_t SizeT;

    Payload() : bytes_(nullptr) {}
    ~Payload() { clear(); }

    Payload(const uint8_t* src, size_t size)
    {
        assert(size == uint16_t(size));

        std::unique_ptr<uint8_t[]> bytes(new uint8_t[size + sizeof(SizeT)]);

        pSize(bytes.get()) = size;
        std::copy(src, src + size, bytes.get() + 2);

        bytes_ = bytes.release() + 2;
    }

    Payload(const Payload& other) { copy(other); }

    Payload& operator= (const Payload& other)
    {
        clear();
        copy(other);
        return *this;
    }

    Payload(Payload&& other) : bytes_(other.bytes_)
    {
        other.bytes_ = nullptr;
    }

    Payload& operator= (Payload&& other)
    {
        clear();
        std::swap(bytes_, other.bytes_);
        return *this;
    }

    void clear()
    {
        if (!bytes_) return;
        delete[] start();
        bytes_ = nullptr;
    }

    const uint8_t* bytes() const { return bytes_; }
    size_t size() const { return *reinterpret_cast<const SizeT*>(start()); }

    const uint8_t* packet() const { return start(); }
    size_t packetSize() const { return pSize(start()) + sizeof(SizeT); }

private:

    void copy(const Payload& other)
    {
        SizeT size = other.size();
        const uint8_t* start = other.start();

        std::unique_ptr<uint8_t[]> bytes(new uint8_t[size + sizeof(SizeT)]);
        std::copy(start, start + size, bytes.get());

        bytes_ = bytes.release() + 2;
        assert(this->size() == other.size());
    }

    uint8_t* start() { return bytes_ - sizeof(SizeT); }
    uint8_t* start() const { return bytes_ - sizeof(SizeT); }

    SizeT& pSize(uint8_t* start) const
    {
        return *reinterpret_cast<SizeT*>(start);
    }

    uint8_t* bytes_;
};


/******************************************************************************/
/* PROTOCOLS                                                                  */
/******************************************************************************/

namespace proto {

#if 0 // \todo Need to update the implementation
Payload fromChunkedHttp(const Payload& msg);
Payload toChunkedHttp(const Payload& msg);
#endif

inline std::string toString(const Payload& msg)
{
    return std::string((char*) msg.bytes(), msg.size());
}

inline Payload fromString(const char* msg, size_t size)
{
    return Payload(reinterpret_cast<const uint8_t*>(msg), size);
}

inline Payload fromString(const std::string& msg)
{
    return fromString(msg.c_str(), msg.size());
}

inline Payload fromString(const char* msg)
{
    return fromString(msg, std::strlen(msg));
}


} // namespace pl


} // slick
