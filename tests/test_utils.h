/* test_utils.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 07 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Test utilities.
*/

#pragma once

namespace slick {


/******************************************************************************/
/* UNWRAP                                                                     */
/******************************************************************************/

template<typename T>
struct Unwrap
{
    typedef T Ret;
    static Ret get(const T& val) { return val; }
};

// Allows printout of std::atomics without having to load them out first.
template<typename T>
struct Unwrap< std::atomic<T> >
{
    typedef T Ret;
    static Ret get(const std::atomic<T>& val) { return val; }
};

template<typename T>
typename Unwrap<T>::Ret unwrap(const T& val) { return Unwrap<T>::get(val); }



/******************************************************************************/
/* FORMAT                                                                     */
/******************************************************************************/
// \todo GCC has a printf param check builtin. No idea if it works with variadic
// templates.

template<typename... Args>
std::string format(const std::string& pattern, const Args&... args)
{
    return format(pattern.c_str(), args...);
}

template<typename... Args>
std::string format(const char* pattern, const Args&... args)
{
    std::array<char, 1024> buffer;

    size_t chars = snprintf(
            buffer.data(), buffer.size(), pattern, unwrap(args)...);

    return std::string(buffer.data(), chars);
}


/******************************************************************************/
/* FORMAT UTILS                                                               */
/******************************************************************************/

std::string fmtElapsedSmall(double elapsed)
{
    static const std::string scaleIndicators = "smunpf?";

    size_t i = 0;
    while (elapsed < 1.0 && i < (scaleIndicators.size() - 1)) {
        elapsed *= 1000.0;
        i++;
    }

    return format("%6.2f%c", elapsed, scaleIndicators[i]);
}

std::string fmtElapsedLarge(double elapsed)
{
    char indicator = 's';

    if (elapsed >= 60.0) {
        elapsed /= 60.0;
        indicator = 'M';
    }
    else goto done;

    if (elapsed >= 60.0) {
        elapsed /= 60.0;
        indicator = 'H';
    }
    else goto done;

    if (elapsed >= 24.0) {
        elapsed /= 24.0;
        indicator = 'D';
    }
    else goto done;

  done:
    return format("%6.2f%c", elapsed, indicator);
}

std::string fmtElapsed(double elapsed)
{
    if (elapsed < 60.0)
        return fmtElapsedSmall(elapsed);
    return fmtElapsedLarge(elapsed);
}


std::string fmtValue(double value)
{
    static const std::string scaleIndicators = " kmgth?";

    size_t i = 0;
    while (value >= 1000.0 && i < (scaleIndicators.size() - 1)) {
        value /= 1000.0;
        i++;
    }

    return format("%6.2f%c", value, scaleIndicators[i]);
}

std::string fmtRatio(double num, double denom)
{
    double value = (num / denom) * 100;
    return format("%6.2f%%", value);
}


std::string fmtTitle(const std::string& title, char fill = '-')
{
    std::string filler(80 - title.size() - 4, fill);
    return format("[ %s ]%s", title.c_str(), filler.c_str());
}

void printTitle(const std::string& title, char fill = '-')
{
    std::string str = fmtTitle(title, fill);
    printf("%s\n", str.c_str());
}




/******************************************************************************/
/* TO STRING                                                                  */
/******************************************************************************/

template<typename Iterator>
std::string toString(Iterator it, Iterator last);

template<typename Cont>
std::string toStringCont(const Cont& c)
{
    return toString(begin(c), end(c));
}

} // namespace slick


namespace std {

inline std::string to_string(const std::string& str) { return str; }

template<typename T>
std::string to_string(T* p)
{
    return slick::format("%p", p);
}

template<typename T>
std::string to_string(const T* p)
{
    return slick::format("%p", p);
}

template<typename First, typename Second>
std::string to_string(const std::pair<First, Second>& p)
{
    return format("<%s, %s>",
            to_string(p.first).c_str(),
            to_string(p.second).c_str());
}


template<typename T, size_t N> struct array;
template<typename T, size_t N>
std::string to_string(const std::array<T, N>& a)
{
    return slick::toString(begin(a), end(a));
}

} // namespace std

namespace slick {

// Needs to reside down here so the to_string(std::pair) overload is visible.
template<typename Iterator>
std::string toString(Iterator it, Iterator last)
{
    std::string str = "[ ";
    for (; it != last; ++it)
        str += std::to_string(*it) + " ";
    str += "]";
    return str;
}

} // namespace slick
