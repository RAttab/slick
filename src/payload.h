/* payload.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Payload seraizlization utilities.

   Might want to consider just using shared_ptr to make everything much simpler.
*/

#include <cstring>
#include <cstdlib>

namespace slick {


/******************************************************************************/
/* MESSAGE                                                                    */
/******************************************************************************/

struct TakeOwnership {};

struct Payload
{
    Payload() : size_(0), bytes_(nullptr) {}

    Payload(const uint8_t* src, size_t size);

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

    ~Payload()
    {
        free(bytes_);
    }

    const uint8_t* bytes() const { return bytes_; }
    size_t size() const { return size_; }

private:
    size_t size_;
    const uint8_t* bytes_;
};


/******************************************************************************/
/* PROTOCOLS                                                                  */
/******************************************************************************/

Payload&& toChunkedHttp(const Payload& msg);
Payload&& fromChunkedHttp(const Payload& msg);

} // slick
