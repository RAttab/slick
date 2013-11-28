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
/* ACTIVE SOCKET                                                              */
/******************************************************************************/

struct ActiveSocket
{
    explicit ActiveSocket(const char* host, int flags = 0);
    ~ActiveSocket();

    ActiveSocket(const ActiveSocket&) = delete;
    ActiveSocket& operator=(const ActiveSocket&) = delete;

    int fd() const { return fd_; }

    static ActiveSocket&& accept(int passiveFd, int flags = 0);

private:
    explicit ActiveSocket() : fd_(-1) {}
    void init();

    int fd_;
    struct sockaddr addr;
    socklen_t addrlen;
};


/******************************************************************************/
/* PASSIVE SOCKETS                                                            */
/******************************************************************************/

struct PassiveSockets
{
    explicit PassiveSockets(const char* port, int flags = 0);
    ~PassiveSockets();

    PassiveSockets(const PassiveSockets&) = delete;
    PassiveSockets& operator=(const PassiveSockets&) = delete;

    const std::vector<int>& fds() { return fds; }
    bool test(int fd)
    {
        return find(fds_.begin(), fds_.end(), fd) != fds_.end();
    };

private:
    std::vector<int> fds_;
};

} // slick
