/* payload.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Payload seraizlization utilities.

   Might want to consider just using shared_ptr to make everything much simpler.
*/

#pragma once

#include <memory>
#include <algorithm>
#include <cassert>


namespace slick {

/******************************************************************************/
/* PAYLOAD                                                                    */
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
    explicit Payload(uint8_t* bytes) :  bytes_(bytes + sizeof(SizeT)) {}
    ~Payload() { clear(); }

    Payload(const Payload& other) { copy(other); }
    Payload& operator= (const Payload& other)
    {
        clear();
        copy(other);
        return *this;
    }

    Payload(Payload&& other) noexcept : bytes_(other.bytes_)
    {
        other.bytes_ = nullptr;
    }
    Payload& operator= (Payload&& other) noexcept
    {
        clear();
        std::swap(bytes_, other.bytes_);
        return *this;
    }


    static Payload read(const uint8_t* buffer, size_t bufferSize);

    void clear()
    {
        if (!bytes_) return;
        delete[] start();
        bytes_ = nullptr;
    }

    const uint8_t* bytes() const { return bytes_; }
    size_t size() const { return *reinterpret_cast<const SizeT*>(start()); }

    const uint8_t* packet() const { return bytes_ ? start() : nullptr; }
    size_t packetSize() const
    {
        return bytes_ ? pSize(start()) + sizeof(SizeT) : 0ULL;
    }

private:

    void copy(const Payload& other);

    uint8_t* start() { return bytes_ - sizeof(SizeT); }
    uint8_t* start() const { return bytes_ - sizeof(SizeT); }

    SizeT& pSize(uint8_t* start) const
    {
        return *reinterpret_cast<SizeT*>(start);
    }

    uint8_t* bytes_;
};

} // slick
