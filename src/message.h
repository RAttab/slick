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

struct Message
{
    Message(size_t size, const uint8_t* bytes) : 
        size(size), bytes(bytes), owned(false)
    {}

    Message(size_t size, uint8_t* src) : 
        size(size), 
        bytes(malloc(size)),
        owned(true)
    {
        std::memcpy(src, bytes, size);
    }

    ~Message()
    {
        if (owned) free(bytes);
    }


private:
    size_t size;
    const uint8_t* bytes;
    bool owned;
};

} // slick
