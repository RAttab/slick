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
#include <endian.h> // linux specific

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

    const uint8_t* packet() const { return bytes_ ? start() : nullptr; }
    size_t packetSize() const
    {
        return bytes_ ? pSize(start()) + sizeof(SizeT) : 0ULL;
    }

private:

    void copy(const Payload& other)
    {
        SizeT size = other.packetSize();
        const uint8_t* start = other.packet();

        std::unique_ptr<uint8_t[]> bytes(new uint8_t[size]);
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
    return std::string((const char*) msg.bytes(), msg.size());
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

Payload fromBuffer(const uint8_t* buffer, size_t bufferSize);


inline uint8_t  hton(uint8_t  val) { return val; }
inline uint16_t hton(uint16_t val) { return htobe16(val); }
inline uint32_t hton(uint32_t val) { return htobe32(val); }
inline uint64_t hton(uint64_t val) { return htobe64(val); }

inline uint8_t  ntoh(uint8_t  val) { return val; }
inline uint16_t ntoh(uint16_t val) { return be16toh(val); }
inline uint32_t ntoh(uint32_t val) { return be32toh(val); }
inline uint64_t ntoh(uint64_t val) { return be64toh(val); }


template<typename Int>
Int toInt(const Payload& pl)
{
    slickStaticAssert(std::is_integral<Int>::value);
    assert(pl.size() == sizeof(Int));

    Int val = *reinterpret_cast<const Int*>(pl.bytes());
    return ::slick::proto::ntoh(val);
}


template<typename Int>
Payload fromInt(Int val)
{
    slickStaticAssert(std::is_integral<Int>::value);

    val = ::slick::proto::hton(val);
    return Payload(reinterpret_cast<const uint8_t*>(&val), sizeof(Int));
}

} // namespace pl


} // slick
