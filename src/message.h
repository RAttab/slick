/* message.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 17 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Message seraizlization utilities.
*/

#include <cstring>
#include <cstdlib>

namespace slick {


/******************************************************************************/
/* MESSAGE                                                                    */
/******************************************************************************/

struct TakeOwnership {};

struct Message
{
    Message() : size_(0), bytes_(nullptr) {}

    Message(const uint8_t* src, size_t size);

    Message(TakeOwnership, const uint8_t* src, size_t size) :
        size_(size), bytes_(src)
    {}

    Message(const Message& other)
    {
        *this = std::move(Message(other.bytes_, other.size_));
    }

    const Message& operator= (const Message& other)
    {
        *this = std::move(Message(other.bytes_, other.size_));
    }

    ~Message()
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

Message&& toChunkedHttp(const Message& msg);
Message&& fromChunkedHttp(const Message& msg);

} // slick
