/* socket.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 18 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Socket abstraction
*/


namespace slick {

/******************************************************************************/
/* PASSIVE SOCKETS                                                            */
/******************************************************************************/

struct PassiveSockets
{
    PassiveSockets(const char* port, int flags = 0);
    ~PassiveSockets();

    PassiveSockets(const PassiveSockets&) = delete;
    PassiveSockets& operator=(const PassiveSockets&) = delete;

    const std::vector<int>& fds() { return fds; }

private:
    std::vector<int> fds;
};


} // slick
