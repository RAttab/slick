/* pack_test.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 29 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   packing framework test
*/

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include <iostream>

#include "pack.h"
#include "lockless/format.h"

#include <boost/test/unit_test.hpp>


using namespace std;
using namespace slick;
using namespace lockless;


/******************************************************************************/
/* INTS                                                                       */
/******************************************************************************/

template<typename Int>
void testInt()
{
    Int value = 0;
    for (size_t i = 0; i < sizeof(Int); ++i)
        value = (value << 8) | (0xA0 + i);

    Int result = unpack<Int>(pack(value));
    BOOST_CHECK_EQUAL(value, result);
}

BOOST_AUTO_TEST_CASE(ints)
{
    testInt< uint8_t>();
    testInt<  int8_t>();
    testInt<uint16_t>();
    testInt< int16_t>();
    testInt<uint32_t>();
    testInt< int32_t>();
    testInt<uint64_t>();
    testInt< int64_t>();
}


/******************************************************************************/
/* FLOATS                                                                     */
/******************************************************************************/

template<typename T>
void testFloat()
{
    T value = 0.1;
    T result = unpack<T>(pack(value));
    BOOST_CHECK_EQUAL(value, result);
}

BOOST_AUTO_TEST_CASE(floats)
{
    testFloat<float>();
    testFloat<double>();
}


/******************************************************************************/
/* STRINGS                                                                    */
/******************************************************************************/

BOOST_AUTO_TEST_CASE(strings)
{
    {
        std::string value = "Blah";
        std::string result = unpack<std::string>(pack(value));
        BOOST_CHECK_EQUAL(value.size(), result.size());
        BOOST_CHECK_EQUAL(value, result);
    }

    {
        std::string result = unpack<std::string>(pack("bleh"));
        BOOST_CHECK_EQUAL("bleh", result);
    }

    {
        const char* value = "blooh";
        std::string result = unpack<std::string>(pack(value));
        BOOST_CHECK_EQUAL("blooh", result);
    }
}


/******************************************************************************/
/* VECTOR                                                                     */
/******************************************************************************/

template<typename T>
void check(const std::vector<T>& value)
{
    auto result = unpack< std::vector<T> >(pack(value));
    BOOST_CHECK_EQUAL(value.size(), result.size());

    bool eq = std::equal(value.begin(), value.end(), result.begin());
    BOOST_CHECK(eq);
}

BOOST_AUTO_TEST_CASE(vectors)
{
    check(std::vector<size_t>{ 1, 2, 20, 1ULL << 52 });
    check(std::vector<std::string>{ "weeeeee", "woooooo", "a", "blehohasd" });

    check(std::vector< std::vector<std::string> >{
                { "blah", "bleeh", "blooooh" },
                { "wee", "wheee", "whoooooo", "whoooooosh" }
            });
}
