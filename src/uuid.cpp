/* uuid.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 03 Jan 2014
   FreeBSD-style copyright and disclaimer apply

   UUID implementation
*/

#include "uuid.h"
#include "pack.h"
#include "lockless/tm.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

namespace slick {


/******************************************************************************/
/* UTILS                                                                      */
/******************************************************************************/
/** \todo All of this could be useful elsewhere. Should be generalized. */

namespace {

template<typename T>
void read(int fd, T* buffer, size_t n)
{
    size_t bytes = n * sizeof(T);
    auto data = reinterpret_cast<uint8_t*>(buffer);

    while (bytes > 0) {
        ssize_t ret = ::read(fd, data, bytes);
        if (ret < 0) {
            if (errno == EINTR) continue;
            SLICK_CHECK_ERRNO(ret >= 0, "UUID.read");
        }

        assert(ret);
        bytes -= ret;
        data += ret;
    }
}

bool linuxRandom(uint8_t* bytes, size_t n)
{
    static int fd = 0;
    if (!fd) {
        fd = open("/dev/random", O_RDONLY);
        if (fd < 0)
            fd = open("/dev/urandom", O_RDONLY);
    }
    if (fd < 0) return false;

    slick::read(fd, bytes, n);
    return true;
}

void userspaceRandom(uint8_t* bytes_)
{
    static std::mt19937_64 rng(lockless::rdtsc());
    std::uniform_int_distribution<uint64_t> dist(0, uint64_t(-1));

    uint64_t* bytes = reinterpret_cast<uint64_t*>(bytes_);
    bytes[0] = dist(rng);
    bytes[1] = dist(rng);
}

template<typename Int, typename It>
It readHex(Int& value, It first, It last)
{
    assert(first + sizeof(value) * 2 <= last);
    (void) last;

    value = 0;

    auto it = first;
    for (it = first; it < first + sizeof(value) * 2; ++it) {
        Int hex;

        if (*it >= '0' && *it <= '9') hex = '0' - *it;
        else if (*it >= 'a' && *it <= 'f') hex = 'a' - *it + 10;
        else if (*it >= 'A' && *it <= 'F') hex = 'A' - *it + 10;
        else return it;


        value = (value << 4) + hex;
    }

    return it;
}

} // namespace anonymous


/******************************************************************************/
/* UUID                                                                       */
/******************************************************************************/

UUID::
UUID()
{
    memset(this, 0, sizeof(UUID));
}

UUID::
UUID(const char* first, size_t n)
{
    auto it = first, last = first + n;

    it = readHex(time_low, it, last);
    it = readHex(time_mid, it, last);
    it = readHex(time_hi_and_version, it, last);
    it = readHex(clk_seq_hi_res, it, last);
    it = readHex(clk_seq_low, it, last);

    for (size_t i = 0; i < sizeof(node); ++i)
        it = readHex(node[i], it, last);
}


UUID::
UUID(const std::string& str)
{
    // \todo A bit ridiculous but I'm feeling lazy right now.
    *this = UUID(str.c_str(), str.size());
}

std::string
UUID::
toString() const
{
    return lockless::format("%x-%x-%x-%x%x-%x%x%x%x%x%x",
            uint32_t(time_low), uint32_t(time_mid), uint32_t(time_hi_and_version),
            uint32_t(clk_seq_hi_res), uint32_t(clk_seq_low),
            uint32_t(node[0]), uint32_t(node[1]), uint32_t(node[2]),
            uint32_t(node[3]), uint32_t(node[4]), uint32_t(node[5]));
}

UUID
UUID::
random()
{
    UUID uuid;
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&uuid);
    size_t n = sizeof(UUID);

    if (!linuxRandom(bytes, n))
        userspaceRandom(bytes);

    return std::move(uuid);
}

UUID
UUID::
time()
{
    static int fd = 0;
    if (!fd) {
        fd = open("/proc/sys/kernel/random/uuid", O_RDONLY);
        SLICK_CHECK_ERRNO(fd >= 0, "UUID.time.kernel");
    }
    if (fd < 0) throw std::runtime_error("kernel uuid source unavailable");

    char buffer[32 + 4];
    slick::read(fd, buffer, sizeof(buffer));
    return UUID(buffer, sizeof(buffer));
}

} // slick
