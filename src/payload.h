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
#include <cstring>
#include <cstdlib>
#include <cstdint>

namespace slick {


/******************************************************************************/
/* MESSAGE                                                                    */
/******************************************************************************/

extern struct TakeOwnershipT {} TakeOwnership;

struct Payload
{
    ~Payload() { std::free(bytes_); }

    Payload() : size_(0), bytes_(nullptr) {}

    Payload(const uint8_t* src, size_t size) :
        size_(size)
    {
        bytes_ = (uint8_t*) std::malloc(size);
        std::memcpy((void*) src, (void*) bytes_, size);
    }

    Payload(TakeOwnershipT, uint8_t* src, size_t size) :
        size_(size), bytes_(src)
    {}

    Payload(const Payload& other)
    {
        *this = std::move(Payload(other.bytes_, other.size_));
    }

    Payload& operator= (const Payload& other)
    {
        *this = std::move(Payload(other.bytes_, other.size_));
        return *this;
    }

    uint8_t* bytes() const { return bytes_; }
    size_t size() const { return size_; }

private:
    size_t size_;
    uint8_t* bytes_;

public:

    /**************************************************************************/
    /* PROTOCOLS                                                              */
    /**************************************************************************/

    static Payload fromChunkedHttp(const Payload& msg);
    static Payload toChunkedHttp(const Payload& msg);

    static std::string toString(const Payload& msg)
    {
        return std::string((char*) msg.bytes(), msg.size());
    }

    static Payload fromString(const std::string& msg)
    {
        return fromString(msg.c_str(), msg.size());
    }

    static Payload fromString(const char* msg)
    {
        return fromString(msg, std::strlen(msg));
    }

    static Payload fromString(const char* msg, size_t size)
    {
        std::unique_ptr<uint8_t> bytes((uint8_t*) std::malloc(size));
        std::memcpy(bytes.get(), msg, size);
        return Payload(TakeOwnership, bytes.release(), size);
    }

};


} // slick
