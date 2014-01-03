/* uuid.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 02 Jan 2014
   FreeBSD-style copyright and disclaimer apply

   UUID generator.
*/

#pragma once

#include "pack.h"
#include "utils.h"
#include "lockless/utils.h"

#include <string>
#include <algorithm>
#include <iterator>

namespace slick {


/******************************************************************************/
/* UUID                                                                       */
/******************************************************************************/

struct UUID
{
    uint32_t time_low;
    uint16_t time_mid;
    uint16_t time_hi_and_version;
    uint8_t  clk_seq_hi_res;
    uint8_t  clk_seq_low;
    uint8_t  node[6];

    UUID();
    UUID(const char* str, size_t n);
    UUID(const std::string& str);
    std::string toString() const;

    static UUID random();
    static UUID time();

    bool operator<(const UUID& other) const
    {
        if (time_low != other.time_low) return time_low < other.time_low;
        if (time_mid != other.time_mid) return time_mid < other.time_mid;
        if (time_hi_and_version != other.time_hi_and_version)
            return time_hi_and_version < other.time_hi_and_version;

        if (clk_seq_hi_res != other.clk_seq_hi_res)
            return clk_seq_hi_res < other.clk_seq_hi_res;

        if (clk_seq_low != other.clk_seq_low)
            return clk_seq_low < other.clk_seq_low;

        for (size_t i = 0; i < sizeof(node); ++i)
            if (node[i] != other.node[i])
                return node[i] < other.node[i];

        return false;
    }

    bool operator==(const UUID& other) const
    {
        return time_low == other.time_low
            && time_mid == other.time_mid
            && time_hi_and_version == other.time_hi_and_version
            && clk_seq_hi_res == other.clk_seq_hi_res
            && clk_seq_low == other.clk_seq_low
            && std::equal(std::begin(node), std::end(node), std::begin(other.node));
    }

    bool operator!=(const UUID& other) const
    {
        return !operator==(other);
    }

};

locklessStaticAssert(sizeof(UUID) == 16);


template<>
struct Pack<UUID>
{
    static size_t size(const UUID&)
    {
        return sizeof(UUID);
    }

    static void pack(const UUID& value, PackIt first, PackIt last)
    {
        packAll(first, last,
                value.time_low, value.time_mid, value.time_hi_and_version,
                value.clk_seq_hi_res, value.clk_seq_low,
                value.node[0], value.node[1], value.node[2],
                value.node[3], value.node[4], value.node[5]);
    }

    static UUID unpack(ConstPackIt first, ConstPackIt last)
    {
        UUID value;
        unpackAll(first, last,
                value.time_low, value.time_mid, value.time_hi_and_version,
                value.clk_seq_hi_res, value.clk_seq_low,
                value.node[0], value.node[1], value.node[2],
                value.node[3], value.node[4], value.node[5]);
        return std::move(value);
    }
};

} // slick


/******************************************************************************/
/* HASH                                                                       */
/******************************************************************************/

namespace std {

template<>
struct hash<slick::UUID>
{
    typedef slick::UUID argument_type;
    typedef size_t result_type;

    size_t operator()(const slick::UUID& value) const noexcept
    {
        size_t hash = 0;

        slick::hash_combine(hash, value.time_low);
        slick::hash_combine(hash, value.time_mid);
        slick::hash_combine(hash, value.time_hi_and_version);
        slick::hash_combine(hash, value.clk_seq_hi_res);
        slick::hash_combine(hash, value.clk_seq_low);

        for (size_t i = 0; i < sizeof(value.node); ++i)
            slick::hash_combine(hash, value.node[i]);

        return hash;
    }
};

} // namespace std
