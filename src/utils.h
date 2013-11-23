/* utils.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 18 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   random utiltiies
*/



/******************************************************************************/
/* SLICK CHECK                                                                */
/******************************************************************************/

#define SLICK_CHECK_ERRNO(pred,msg)                             \
    do {                                                        \
        if (!(pred))                                            \
            throw std::string(msg) + ": " + strerror(errno);    \
    } while(false);                                             \


/******************************************************************************/
/* THREAD ID                                                                  */
/******************************************************************************/

// Thread local storage.
#define slickTls __thread __attribute__(( tls_model("initial-exec") ))

namespace slick {

size_t threadId();

} // namespace slick
