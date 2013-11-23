/* socket.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 18 Nov 2013
   FreeBSD-style copyright and disclaimer apply

   Socket abstraction
*/


namespace slick {

/******************************************************************************/
/* GUARD                                                                      */
/******************************************************************************/

struct FdGuard
{
    FdGuard(int fd) : fd(fd) {}
    ~FdGuard() { if (fd >= 0) close(fd); }

    int release()
    {
        int old = fd;
        fd = -1;
        return old;
    }

private:
    int fd;
};

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
    bool test(int fd)
    {
        return find(fd.begin(), fd.end(), fd) != fd.end();
    };

private:
    int setOptions(int fd);

    std::vector<int> fds;
};


} // slick
