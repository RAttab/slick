/* pack.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 28 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Packing framework
*/

#pragma once

#include "payload.h"

#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <type_traits>
#include <cassert>
#include <endian.h> // linux specific


namespace slick {


/******************************************************************************/
/* ENDIAN                                                                     */
/******************************************************************************/

inline uint8_t  hton(uint8_t  val) { return val; }
inline uint16_t hton(uint16_t val) { return htobe16(val); }
inline uint32_t hton(uint32_t val) { return htobe32(val); }
inline uint64_t hton(uint64_t val) { return htobe64(val); }

inline uint8_t  ntoh(uint8_t  val) { return val; }
inline uint16_t ntoh(uint16_t val) { return be16toh(val); }
inline uint32_t ntoh(uint32_t val) { return be32toh(val); }
inline uint64_t ntoh(uint64_t val) { return be64toh(val); }


/******************************************************************************/
/* PACK                                                                       */
/******************************************************************************/

template<typename T, typename Enable = void> struct Pack;

typedef uint8_t* PackIt;
typedef const uint8_t* ConstPackIt;


template<typename T>
Payload pack(const T& value)
{
    size_t dataSize = Pack<T>::size(value);
    size_t totalSize = dataSize + sizeof(Payload::SizeT);
    std::unique_ptr<uint8_t[]> bytes(new uint8_t[totalSize]);

    *reinterpret_cast<Payload::SizeT*>(bytes.get()) = dataSize;

    PackIt first = bytes.get() + sizeof(Payload::SizeT);
    PackIt last = first + dataSize;
    Pack<T>::pack(value, first, last);

    return Payload(bytes.release());
}

template<typename T>
T unpack(const Payload& data)
{
    ConstPackIt first = data.bytes();
    ConstPackIt last = first + data.size();

    return Pack<T>::unpack(first, last);
}


/******************************************************************************/
/* INTEGERS                                                                   */
/******************************************************************************/

template<typename Int>
struct Pack<Int, typename std::enable_if< std::is_integral<Int>::value >::type>
{
    static constexpr size_t size(Int) { return sizeof(Int); }

    static void pack(Int value, PackIt first, PackIt last)
    {
        assert(size_t(last - first) >= sizeof(Int));
        *reinterpret_cast<Int*>(first) = hton(value);
    }

    static Int unpack(ConstPackIt first, ConstPackIt last)
    {
        assert(size_t(last - first) >= sizeof(Int));

        Int value = *reinterpret_cast<const Int*>(first);
        return ntoh(value);
    }

};


/******************************************************************************/
/* STRING                                                                     */
/******************************************************************************/

template<>
struct Pack<std::string>
{
    static size_t size(const std::string& value) { return value.size() + 1; }

    static void pack(const std::string& value, PackIt first, PackIt last)
    {
        assert(size_t(last - first) >= value.size() + 1);

        std::copy(value.begin(), value.end(), first);
        *(first + value.size()) = '\0';
    }

    static std::string unpack(ConstPackIt first, ConstPackIt last)
    {
        std::string value(reinterpret_cast<const char*>(first), last - first);

        assert(*(first + value.size()) == '\0');

        return std::move(value);
    }
};


/******************************************************************************/
/* VECTOR                                                                     */
/******************************************************************************/

template<typename T>
struct Pack< std::vector<T> >
{
    static size_t size(const std::vector<T>& value)
    {
        // \todo Confirm that the compiler is capable of optimizing out the loop
        // for fixed width types. Not sure if it's smart enough to figure out
        // the for-each loop; might need to switch to a plain old for i loop.

        size_t size = sizeof(Payload::SizeT);
        for (const auto& item: value) size += Pack<T>::size(item);
        return size;
    }

    static void pack(const std::vector<T>& value, PackIt first, PackIt last)
    {
        *reinterpret_cast<Payload::SizeT*>(first) = value.size();

        PackIt it = first + sizeof(Payload::SizeT);
        for (const auto& item : value) {
            size_t size = Pack<T>::size(item);
            assert(it + size < last);

            Pack<T>::pack(item, it, last);
            it += size;
        }
    }

    static std::vector<T> unpack(ConstPackIt first, ConstPackIt last)
    {
        size_t size = *reinterpret_cast<Payload::SizeT*>(first);

        std::vector<T> value;
        value.reserve(size);

        PackIt it = first + sizeof(Payload::SizeT);
        for (size_t i = 0; i < size; ++i) {
            T item = Pack<T>::unpack(it, last);

            it += Pack<T>::size(item);
            assert(it < last);

            value.emplace_back(std::move(item));
        }

        return std::move(value);
    }

};

} // slick
