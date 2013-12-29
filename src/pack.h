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
#include <cstring>
#include <endian.h> // linux specific


namespace slick {


/******************************************************************************/
/* ENDIAN                                                                     */
/******************************************************************************/

namespace details {

template<typename T, size_t N>
struct IsSizedNumber
{
    static constexpr bool value =
        std::is_integral<T>::value && sizeof(T) == N;
};

template<size_t N> struct SizedInt;
template<> struct SizedInt<4> { typedef uint32_t type; };
template<> struct SizedInt<8> { typedef uint64_t type; };

} // namespace details


template<typename T>
T hton(T val, typename std::enable_if< details::IsSizedNumber<T, 1>::value >::type* = 0)
{
    return val;
}

template<typename T>
T hton(T val, typename std::enable_if< details::IsSizedNumber<T, 2>::value >::type* = 0)
{
    return htobe16(val);
}

template<typename T>
T hton(T val, typename std::enable_if< details::IsSizedNumber<T, 4>::value >::type* = 0)
{
    return htobe32(val);
}

template<typename T>
T hton(T val, typename std::enable_if< details::IsSizedNumber<T, 8>::value >::type* = 0)
{
    return htobe64(val);
}

template<typename T>
T hton(T val, typename std::enable_if< std::is_floating_point<T>::value >::type* = 0)
{
    union {
        T orig;
        typename details::SizedInt<sizeof(T)>::type raw;
    } punt;

    punt.orig = val;
    punt.raw = hton(punt.raw);
    return punt.orig;
}



template<typename T>
T ntoh(T val, typename std::enable_if< details::IsSizedNumber<T, 1>::value >::type* = 0)
{
    return val;
}

template<typename T>
T ntoh(T val, typename std::enable_if< details::IsSizedNumber<T, 2>::value >::type* = 0)
{
    return be16toh(val);
}

template<typename T>
T ntoh(T val, typename std::enable_if< details::IsSizedNumber<T, 4>::value >::type* = 0)
{
    return be32toh(val);
}

template<typename T>
T ntoh(T val, typename std::enable_if< details::IsSizedNumber<T, 8>::value >::type* = 0)
{
    return be64toh(val);
}

template<typename T>
T ntoh(T val, typename std::enable_if< std::is_floating_point<T>::value >::type* = 0)
{
    union {
        T orig;
        typename details::SizedInt<sizeof(T)>::type raw;
    } punt;

    punt.orig = val;
    punt.raw = ntoh(punt.raw);
    return punt.orig;
}


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

    return Payload (bytes.release());
}

template<typename T>
T unpack(const Payload& data)
{
    ConstPackIt first = data.bytes();
    ConstPackIt last = first + data.size();

    return Pack<T>::unpack(first, last);
}


/******************************************************************************/
/* ARITHMETIC TYPES                                                           */
/******************************************************************************/

template<typename T>
struct Pack<T, typename std::enable_if< std::is_arithmetic<T>::value >::type>
{
    static constexpr size_t size(T) { return sizeof(T); }

    static void pack(T value, PackIt first, PackIt last)
    {
        assert(size_t(last - first) >= size(value));
        *reinterpret_cast<T*>(first) = hton(value);
    }

    static T unpack(ConstPackIt first, ConstPackIt last)
    {
        assert(size_t(last - first) >= sizeof(T));
        return ntoh(*reinterpret_cast<const T*>(first));
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
        assert(size_t(last - first) >= size(value));

        std::copy(value.begin(), value.end(), first);
        *(first + value.size()) = '\0';
    }

    static std::string unpack(ConstPackIt first, ConstPackIt last)
    {
        auto it = std::find(first, last, '\0');
        assert(it != last);
        return std::string(reinterpret_cast<const char*>(first), it - first);
    }
};


/******************************************************************************/
/* C STRING                                                                   */
/******************************************************************************/

namespace details {

template<typename T>
struct IsCharPtr
{
    static constexpr bool isCharArray =
        std::is_array<T>::value &&
        std::is_same<char,
            typename std::remove_const<
                typename std::remove_extent<T>::type>::type>::value;

    static constexpr bool isCharPtr =
        std::is_pointer<T>::value &&
        std::is_same<char,
            typename std::remove_const<
                typename std::remove_pointer<T>::type>::type>::value;

    static constexpr bool value = isCharArray || isCharPtr;
};

} // namespace details


/* This is one way only because unpack would not be exception safe. */
template<typename T>
struct Pack<T, typename std::enable_if< details::IsCharPtr<T>::value >::type>
{
    static size_t size(const char* value) { return std::strlen(value) + 1; }
    static void pack(const char* value, PackIt first, PackIt last)
    {
        std::strncpy(reinterpret_cast<char*>(first), value, last - first);
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
            assert(it + size <= last);

            Pack<T>::pack(item, it, last);
            it += size;
        }
    }

    static std::vector<T> unpack(ConstPackIt first, ConstPackIt last)
    {
        size_t size = *reinterpret_cast<const Payload::SizeT*>(first);

        std::vector<T> value;
        value.reserve(size);

        ConstPackIt it = first + sizeof(Payload::SizeT);
        for (size_t i = 0; i < size; ++i) {
            T item = Pack<T>::unpack(it, last);

            it += Pack<T>::size(item);
            assert(it <= last);

            value.emplace_back(std::move(item));
        }

        return std::move(value);
    }

};

} // slick
