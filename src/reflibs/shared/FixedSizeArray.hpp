//
// FixedSizeArray.hpp
//  Simple stack-like compile-time sized array/vector template.
//
#pragma once

#include "Common.hpp"
#include <algorithm>
#include <utility>

namespace MrQ2
{

// Very simple array/stack-like container of fixed size.
// All elements are default constructed on initialization.
// Popping an element doesn't destroy it, it just decrements
// the array used size. Clearing the array just sets size = 0.
// Essentially std::array<T> + a count/size.
template<typename T, int kCapacity>
class FixedSizeArray final
{
public:

    using value_type      = T;
    using size_type       = int;
    using pointer         = T *;
    using const_pointer   = const T *;
    using reference       = T &;
    using const_reference = const T &;

    FixedSizeArray() = default;
    FixedSizeArray(FixedSizeArray &&) = default;
    FixedSizeArray(const FixedSizeArray &) = default;
    FixedSizeArray & operator = (FixedSizeArray &&) = default;
    FixedSizeArray & operator = (const FixedSizeArray &) = default;

    FixedSizeArray(const_pointer first, const size_type count)
    {
        MRQ2_ASSERT(count <= capacity());
        std::copy_n(first, count, data());
        m_count = count;
    }

    FixedSizeArray(const_pointer first, const_pointer last)
    {
        const auto count = static_cast<size_type>(std::distance(first, last));
        MRQ2_ASSERT(count <= capacity());
        std::copy(first, last, data());
        m_count = count;
    }

    template<int ArraySize>
    FixedSizeArray(const value_type (&arr)[ArraySize])
        : FixedSizeArray{ arr, ArraySize }
    {
    }

    void fill(const_reference val, const size_type count = kCapacity)
    {
        MRQ2_ASSERT(count <= capacity());
        std::fill_n(data(), count, val);
        m_count = count;
    }

    void resize(const size_type count, const_reference val)
    {
        fill(val, count);
    }

    void resize(const size_type count)
    {
        m_count = count;
    }

    void clear()
    {
        m_count = 0;
    }

    void push_back(const_reference val)
    {
        MRQ2_ASSERT(size() < capacity());
        m_elements[m_count++] = val;
    }

    void push_back(value_type && val)
    {
        MRQ2_ASSERT(size() < capacity());
        m_elements[m_count++] = std::move(val);
    }

    void pop_back()
    {
        MRQ2_ASSERT(!empty());
        --m_count;
    }

    bool operator==(const FixedSizeArray & other) const
    {
        return std::equal(begin(), end(), other.begin(), other.end());
    }

    bool operator!=(const FixedSizeArray & other) const
    {
        return !(*this == other);
    }

    const_reference operator[](const size_type index) const
    {
        MRQ2_ASSERT(index >= 0 && index < size());
        return m_elements[index];
    }

    reference operator[](const size_type index)
    {
        MRQ2_ASSERT(index >= 0 && index < size());
        return m_elements[index];
    }

    const_reference front() const { MRQ2_ASSERT(!empty()); return m_elements[0]; }
    reference       front()       { MRQ2_ASSERT(!empty()); return m_elements[0]; }

    const_reference back()  const { MRQ2_ASSERT(!empty()); return m_elements[size() - 1]; }
    reference       back()        { MRQ2_ASSERT(!empty()); return m_elements[size() - 1]; }

    size_type       size()  const { return m_count; }
    bool            empty() const { return size() == 0; }

    const_pointer   data()  const { return &m_elements[0]; }
    pointer         data()        { return &m_elements[0]; }

    const_pointer   begin() const { return &m_elements[0]; }
    pointer         begin()       { return &m_elements[0]; }

    const_pointer   end()   const { return &m_elements[size()]; }
    pointer         end()         { return &m_elements[size()]; }

    static constexpr size_type capacity() { return kCapacity; }

private:

    static_assert(kCapacity > 0, "Cannot allocate FixedSizeArray of zero capacity!");

    size_type  m_count = 0;
    value_type m_elements[kCapacity];
};

} // MrQ2
