/* pack.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 28 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Packing framework
*/

#pragma once

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

typedef uint8_t* PackIt;

template<typename T>
Payload pack(const T& value)
{
    size_t dataSize = packSize(value);
    size_t totalSize = dataSize + sizeof(Payload::SizeT);
    std::unique_ptr<uint8_t[]> bytes(new uint8_t[totalSize]);

    *reinterpret_cast<Payload::SizeT*>(bytes.get()) = dataSize;

    PackIt first = bytes.get() + sizeof(Payload::SizeT);
    PackIt last = first + dataSize;
    pack(value, first, last);

    return Payload(bytes.release());
}

template<typename T>
T unpack(const Payload& data)
{
    PackIt first = data.bytes();
    PackIt last = first + data.size();

    return unpack<T>(first, last);
}

/** This is a default. Override for fancy stuff */
template<typename T>
size_t packSize(const T& value)
{
    return sizeof(T);
}


/******************************************************************************/
/* INTEGERS                                                                   */
/******************************************************************************/

template<typename Int>
void pack(Int value, PackIt first, PackIt last,
        typename typename std::enable_if<std::is_integral<Int>::type>::type* = 0)
{
    assert(last - first >= sizeof(Int));

    *reinterpret_cast<Int*>(first) = hton(value);
}

template<typename Int>
Int unpack(PackIt first, PackIt last,
        typename typename std::enable_if<std::is_integral<Int>::type>::type* = 0)
{
    assert(last - first >= sizeof(Int));

    Int value = *reinterpret_cast<Int*>(first);
    return ntoh(value);
}


/******************************************************************************/
/* STRING                                                                     */
/******************************************************************************/

template<>
size_t packSize<std::string>(const std::string& value)
{
    return value.size() + 1;
}

void pack(const std::string& value, PackIt first, PackIt last)
{
    assert(last - first >= value.size() + 1);

    std::copy(value.begin(), value.end(), first);
    *(first + value.size()) = '\0';
}

std::string unpack(PackIt first, PackIt last)
{
    std::string value(first, last - first);

    assert(*(first + value.size()) == '\0');

    return std::move(value);
}


// These are only convenience functions and no unpack is provided for char*.

template<>
size_t packSize<const char*>(const char* value)
{
    return std::strlen(value) + 1;
}

void pack(const char* value, PackIt first, PackIt last)
{
    assert(last - first >= value.size() + 1);

    std::copy(value.begin(), value.end(), first);
    *(first + value.size()) = '\0';
}


/******************************************************************************/
/* VECTOR                                                                     */
/******************************************************************************/

template<typename T>
size_t packSize< std::vector<T> >(const std::vector<T>& value)
{
    // \todo Confirm that the compiler is capable of optimizing out the loop for
    // fixed width types. Not sure if it's smart enough to figure out the
    // for-each loop; might need to switch to a plain old for i loop.

    size_t size = sizeof(Payload::SizeT);
    for (const auto& item: value) size += packSize(item);
    return size;
}

template<typename T>
void pack(const std::vector<T>& value, PackIt first, PackIt last)
{
    *reinterpret_cast<Payload::SizeT*>(first) = value.size();

    PackIt it = first + sizeof(Payload::SizeT);
    for (const auto& item : value) {
        size_t size = packSize(item);
        assert(it + size < last);

        pack(item, it, last);
        it += size;
    }
}

std::vector<T> unpack(PackIt first, PackIt last)
{
    size_t size = *reinterpret_cast<Payload::SizeT*>(first);

    std::vector<T> value;
    value.reserve(size);

    PackIt it = first + sizeof(Payload::SizeT);
    for (size_t i = 0; i < size; ++i) {
        T item = unpack(it, last);

        it += packSize(item);
        assert(it < last);

        value.emplace_back(std::move(item));
    }

    return std::move(value);
}

} // slick
