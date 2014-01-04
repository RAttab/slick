/* stream.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 04 Jan 2014
   FreeBSD-style copyright and disclaimer apply

   Stream utilities
*/

#pragma once

#include <ostream>
#include <sstream>

namespace slick {

/******************************************************************************/
/* PROTOTYPES                                                                 */
/******************************************************************************/

template<typename Arg, typename... Rest>
void streamAll(std::ostream&, const Arg&, const Rest&...);


/******************************************************************************/
/* STREAM CONT                                                                */
/******************************************************************************/


template<typename TupleT, size_t... S>
std::ostream& streamTuple(std::ostream& stream, const TupleT& value, Seq<S...>)
{
    stream << "<";
    streamAll(stream, std::get<S>(value)...);
    stream << ">";
    return stream;
}

template<typename... Args>
std::ostream& operator<<(std::ostream& stream, const std::tuple<Args...>& value)
{
    streamTuple(stream, value, typename GenSeq<sizeof...(Args)>::type());
    return stream;
}


template<typename T>
std::ostream& operator<<(std::ostream& stream, const std::vector<T>& vec)
{
    stream << "[ ";
    for (const auto& val : vec) stream << val << " ";
    stream << "]";
    return stream;
}


template<typename T>
std::ostream& operator<<(std::ostream& stream, const std::set<T>& vec)
{
    stream << "[ ";
    for (const auto& val : vec) stream << val << " ";
    stream << "]";
    return stream;
}

/******************************************************************************/
/* STREAM ALL                                                                 */
/******************************************************************************/

void streamAll(std::ostream&) {}

template<typename Arg, typename... Rest>
void streamAll(std::ostream& stream, const Arg& arg, const Rest&... rest)
{
    stream << arg;
    if (!sizeof...(rest)) return;

    stream << " ";
    streamAll(stream, rest...);
}


} // slick
