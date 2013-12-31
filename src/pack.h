/* pack.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 28 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Serialization framework.

   Note that this assumes that both end of the communication will have the same
   binary representation for floats.
*/

#pragma once

#include "payload.h"
#include "utils.h"

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

typedef Payload::iterator PackIt;
typedef Payload::const_iterator ConstPackIt;


template<typename T>
size_t packedSize(const T& value)
{
    return Pack<T>::size(value);
}


template<typename T>
PackIt pack(const T& value, PackIt first, PackIt last)
{
    Pack<T>::pack(value, first, last);
    return first + packedSize(value);
}

template<typename T>
Payload pack(const T& value)
{
    Payload data(Pack<T>::size(value));
    pack(value, data.begin(), data.end());
    return std::move(data);
}


template<typename T>
T unpack(ConstPackIt first, ConstPackIt last)
{
    return Pack<T>::unpack(first, last);
}

template<typename T>
T unpack(const Payload& data)
{
    return unpack<T>(data.cbegin(), data.cend());
}


template<typename T>
ConstPackIt unpack(T& value, ConstPackIt first, ConstPackIt last)
{
    value = Pack<T>::unpack(first, last);
    return first + packedSize(value);
}

template<typename T>
void unpack(T& value, const Payload& data)
{
    unpack(value, data.cbegin(), data.cend());
}


/******************************************************************************/
/* UTILS                                                                      */
/******************************************************************************/

size_t packedSizeAll() { return 0; }

template<typename Arg, typename... Rest>
size_t packedSizeAll(const Arg& arg, const Rest&... rest)
{
    return packedSize(arg) + packedSizeAll(rest...);
}


PackIt packAll(PackIt first, PackIt) { return first; }

template<typename Arg, typename... Rest>
PackIt packAll(PackIt first, PackIt last, const Arg& arg, const Rest&... rest)
{
    auto it = pack(arg, first, last);
    return packAll(it, last, rest...);
}


ConstPackIt unpackAll(ConstPackIt first, ConstPackIt) { return first; }

template<typename Arg, typename... Rest>
ConstPackIt unpackAll(ConstPackIt first, ConstPackIt last, Arg& arg, Rest&... rest)
{
    auto it = unpack(arg, first, last);
    return unpackAll(it, last, rest...);
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
        assert(size_t(last - first) >= sizeof(T));
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
/* PAIR                                                                       */
/******************************************************************************/

template<typename T1, typename T2>
struct Pack< std::pair<T1, T2> >
{
    typedef std::pair<T1, T2> PairT;

    static size_t size(const PairT& value)
    {
        return packedSizeAll(value.first, value.second);
    }

    static void pack(const PairT& value, PackIt first, PackIt last)
    {
        packAll(first, last, value.first, value.second);
    }

    static PairT unpack(ConstPackIt first, ConstPackIt last)
    {
        PairT value;
        unpackAll(first, last, value.first, value.second);
        return std::move(value);
    }
};


/******************************************************************************/
/* TUPLE                                                                      */
/******************************************************************************/

template<typename... Args>
struct Pack< std::tuple<Args...> >
{
    typedef std::tuple<Args...> TupleT;

    template<size_t... S>
    static size_t size(const TupleT& value, Seq<S...>)
    {
        return packedSizeAll(std::get<S>(value)...);
    }

    static size_t size(const TupleT& value)
    {
        return size(value, typename GenSeq<sizeof...(Args)>::type());
    }


    template<size_t... S>
    static void pack(const TupleT& value, PackIt first, PackIt last, Seq<S...>)
    {
        packAll(first, last, std::get<S>(value)...);
    }

    static void pack(const TupleT& value, PackIt first, PackIt last)
    {
        pack(value, first, last, typename GenSeq<sizeof...(Args)>::type());
    }


    template<size_t... S>
    static void unpack(TupleT& value, ConstPackIt first, ConstPackIt last, Seq<S...>)
    {
        unpackAll(first, last, std::get<S>(value)...);
    }

    static TupleT unpack(ConstPackIt first, ConstPackIt last)
    {
        TupleT value;
        unpack(value, first, last, typename GenSeq<sizeof...(Args)>::type());
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
        for (const auto& item: value) size += packedSize(item);
        return size;
    }

    static void pack(const std::vector<T>& value, PackIt first, PackIt last)
    {
        *reinterpret_cast<Payload::SizeT*>(first) = value.size();

        PackIt it = first + sizeof(Payload::SizeT);
        for (const auto& item : value) {
            it = slick::pack(item, it, last);
            assert(it <= last);
        }
    }

    static std::vector<T> unpack(ConstPackIt first, ConstPackIt last)
    {
        size_t size = *reinterpret_cast<const Payload::SizeT*>(first);

        std::vector<T> value;
        value.reserve(size);

        ConstPackIt it = first + sizeof(Payload::SizeT);
        for (size_t i = 0; i < size; ++i) {
            T item;
            it = slick::unpack(item, it, last);
            assert(it <= last);

            value.emplace_back(std::move(item));
        }

        return std::move(value);
    }

};


/******************************************************************************/
/* PAYLOAD                                                                    */
/******************************************************************************/

template<>
struct Pack<Payload>
{
    static size_t size(const Payload& value)
    {
        return value.packetSize();
    }

    static void pack(const Payload& value, PackIt first, PackIt last)
    {
        assert(first + value.packetSize() <= last);
        *reinterpret_cast<Payload::SizeT*>(first) = value.size();
        std::copy(value.cbegin(), value.cend(), first + sizeof(Payload::SizeT));
    }

    static Payload unpack(ConstPackIt first, ConstPackIt last)
    {
        Payload value(*reinterpret_cast<const Payload::SizeT*>(first));

        auto it = first + sizeof(Payload::SizeT);
        assert(it + value.size() <= last);

        std::copy(it, it + value.size(), value.begin());
        return std::move(value);
    }
};

} // slick
