/* sorted_vector.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 03 Jan 2014
   FreeBSD-style copyright and disclaimer apply

   Sorted vector data structure.
*/

#pragma once

#include <vector>
#include <algorithm>

namespace slick {


/******************************************************************************/
/* SORTED VECTOR                                                              */
/******************************************************************************/

template<typename T, typename Compare = std::less<T> >
struct SortedVector
{
    typedef typename std::vector<T>::iterator iterator;
    typedef typename std::vector<T>::const_iterator const_iterator;

    explicit SortedVector(Compare comp = Compare()) : comp(std::move(comp)) {}

    template<typename Iterator>
    SortedVector(Iterator first, Iterator last, Compare comp = Compare()) :
        vec(first, last), comp(std::move(comp))
    {
        sort();
    }

    SortedVector(const std::initializer_list<T>& init) : vec(init) { sort(); }

    size_t size() const { return vec.size(); }
    bool empty() const { return vec.empty(); }

    void reserve(size_t n) { vec.reserve(n); }
    size_t capacity() const { return vec.capacity(); }

    iterator begin() { return vec.begin(); }
    const_iterator begin() const { return vec.begin(); }
    const_iterator cbegin() const { return vec.begin(); }

    iterator end() { return vec.end(); }
    const_iterator end() const { return vec.end(); }
    const_iterator cend() const { return vec.end(); }

    const T& at(size_t index) const { return vec.at(index); }
    const T& operator[](size_t index) const { return vec[index]; }
    const T& front() const { return vec.front(); }
    const T& back() const { return vec.front(); }
    const T* data() const { return vec.data(); }

    std::pair<iterator, iterator>
    equal_range(const T& value)
    {
        return std::equal_range(begin(), end(), value, comp);
    }

    std::pair<const_iterator, const_iterator>
    equal_range(const T& value) const
    {
        return std::equal_range(begin(), end(), value, comp);
    }

    iterator lower_bound(const T& value)
    {
        return std::lower_bound(begin(), end(), value, comp);
    }

    const_iterator lower_bound(const T& value) const
    {
        return std::lower_bound(begin(), end(), value, comp);
    }

    iterator upper_bound(const T& value)
    {
        return std::upper_bound(begin(), end(), value, comp);
    }

    const_iterator upper_bound(const T& value) const
    {
        return std::upper_bound(begin(), end(), value, comp);
    }

    size_t count(const T& value) const
    {
        auto range = equal_range(value);
        return std::distance(range.first, range.second);
    }

    iterator find(const T& value)
    {
        auto range = equal_range(value);
        return range.first == range.second ? end() : range.first;
    }

    const_iterator find(const T& value) const
    {
        auto range = equal_range(value);
        return range.first == range.second ? end() : range.first;
    }

    void clear() { return vec.clear(); }

    template<typename Arg>
    iterator insert(Arg&& value)
    {
        return vec.insert(upper_bound(value), std::forward<Arg>(value));
    }

    template<typename Iterator>
    void insert(Iterator first, Iterator last)
    {
        vec.insert(vec.end(), first, last);
        sort();
    }

    void insert(const std::initializer_list<T>& init)
    {
        insert(init.begin(), init.end());
    }

    template<typename... Args>
    iterator emplace(Args&&... args)
    {
        T value(std::forward<Args>(args)...);
        return vec.emplace(upper_bound(value), value);
    }

    template<typename Iterator>
    void erase(Iterator it)
    {
        vec.erase(it);
    }

    template<typename Iterator>
    void erase(Iterator first, Iterator last)
    {
        vec.erase(first, last);
    }

    size_t erase(const T& value)
    {
        auto range = equal_range(value);

        size_t n = std::distance(range.first, range.second);
        if (n > 0) erase(range.first, range.second);

        return n;
    }

    void swap(const SortedVector<T>& other)
    {
        vec.swap(other.vec);
    }

private:
    void sort() { std::sort(begin(), end(), comp); }

    std::vector<T> vec;
    Compare comp;
};

} // slick
