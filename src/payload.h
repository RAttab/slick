/* payload.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Payload seraizlization utilities.

   Might want to consider just using shared_ptr to make everything much simpler.
*/


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

struct TakeOwnership {};

struct Payload
{
    ~Payload() { free(bytes_); }

    Payload() : size_(0), bytes_(nullptr) {}

    Payload(const uint8_t* src, size_t size) :
        size_(size), bytes_(std::malloc(size))
    {
        std::memcpy(src, bytes_, size);
    }

    Payload(TakeOwnership, const uint8_t* src, size_t size) :
        size_(size), bytes_(src)
    {}

    Payload(const Payload& other)
    {
        *this = std::move(Payload(other.bytes_, other.size_));
    }

    const Payload& operator= (const Payload& other)
    {
        *this = std::move(Payload(other.bytes_, other.size_));
    }

    const uint8_t* bytes() const { return bytes_; }
    size_t size() const { return size_; }

private:
    size_t size_;
    const uint8_t* bytes_;

public:

    /**************************************************************************/
    /* PROTOCOLS                                                              */
    /**************************************************************************/

    static Payload&& fromChunkedHttp(const Payload& msg);
    static Payload&& toChunkedHttp(const Payload& msg);

    static std::string&& toString(const Payload& msg)
    {
        return std::string(msg.bytes(), msg.size());
    }

    static Payload&& fromString(const std::string& msg)
    {
        return fromString(msg.c_str());
    }

    static Payload&& fromString(const char* msg)
    {
        std::unique_ptr<uint8_t> bytes(std::malloc(msg.size()));
        std::memcpy(bytes, msg.bytes(), msg.size());
        return Payload(TakeOwnership, bytes.release(), msg.size());
    }

};


} // slick
