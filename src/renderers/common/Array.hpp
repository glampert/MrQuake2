//
// Array.hpp
//  Simple compile-time sized array/vector template.
//
#pragma once

#include "Common.hpp"
#include <algorithm>
#include <utility>

namespace MrQ2
{

// Base array_view-style container so we can pass FixedSizeArrays of different sizes as arguments more easily.
template<typename T>
class ArrayBase
{
public:

    using value_type      = T;
    using size_type       = uint32_t;
    using pointer         = T *;
    using const_pointer   = const T *;
    using reference       = T &;
    using const_reference = const T &;

    template<uint32_t kArraySize>
    static ArrayBase<T> from_c_array(T (&arr)[kArraySize])
    {
        return ArrayBase<T>{ arr, kArraySize, kArraySize };
    }

    ArrayBase() // empty
        : m_elements{ nullptr }
        , m_count{ 0 }
        , m_capacity{ 0 }
    { }

    ArrayBase(T * elements, const size_type count, const size_type capacity)
        : m_elements{ elements }
        , m_count{ count }
        , m_capacity{ capacity }
    {
        MRQ2_ASSERT(elements != nullptr);
        MRQ2_ASSERT(count <= capacity);
    }

    void fill(const_reference val, const size_type count)
    {
        MRQ2_ASSERT(count <= m_capacity);
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

    template<typename Pred>
    void erase_if(Pred && predicate)
    {
        if (!empty())
        {
            auto first = begin();
            auto new_end = std::remove_if(first, end(), predicate);
            if (new_end != end())
            {
                auto new_count = size_type(new_end - first);
                MRQ2_ASSERT(new_count <= m_capacity);
                m_count = new_count;
            }
        }
    }

    void erase_swap(pointer erase_iter)
    {
        // move last element into erase_iter
        if (!empty())
        {
            MRQ2_ASSERT(erase_iter >= begin() && erase_iter < end());
            if (m_count > 1)
            {
                T & last = m_elements[m_count - 1];
                *erase_iter = std::move(last);
            }
            --m_count;
        }
    }

    void clear()
    {
        m_count = 0;
    }

    void push_back(const_reference val)
    {
        MRQ2_ASSERT(m_count < m_capacity);
        m_elements[m_count++] = val;
    }

    void push_back(value_type && val)
    {
        MRQ2_ASSERT(m_count < m_capacity);
        m_elements[m_count++] = std::move(val);
    }

    void pop_back()
    {
        MRQ2_ASSERT(!empty());
        --m_count;
    }

    bool operator==(const ArrayBase & other) const
    {
        return std::equal(begin(), end(), other.begin(), other.end());
    }

    bool operator!=(const ArrayBase & other) const
    {
        return !(*this == other);
    }

    const_reference operator[](const size_type index) const
    {
        MRQ2_ASSERT(index < m_count);
        return m_elements[index];
    }

    reference operator[](const size_type index)
    {
        MRQ2_ASSERT(index < m_count);
        return m_elements[index];
    }

    const_reference front()    const { MRQ2_ASSERT(!empty()); return m_elements[0]; }
    reference       front()          { MRQ2_ASSERT(!empty()); return m_elements[0]; }

    const_reference back()     const { MRQ2_ASSERT(!empty()); return m_elements[m_count - 1]; }
    reference       back()           { MRQ2_ASSERT(!empty()); return m_elements[m_count - 1]; }

    size_type       size()     const { return m_count; }
    size_type       capacity() const { return m_capacity; }
    bool            empty()    const { return m_count == 0; }

    const_pointer   data()     const { return m_elements; }
    pointer         data()           { return m_elements; }

    const_pointer   begin()    const { return m_elements; }
    pointer         begin()          { return m_elements; }

    const_pointer   end()      const { return &m_elements[m_count]; }
    pointer         end()            { return &m_elements[m_count]; }

protected:

    value_type * const m_elements;
    size_type          m_count;
    const size_type    m_capacity;
};

// Simple array/stack-like container of fixed size.
// All elements are default constructed on initialization.
// Popping an element doesn't destroy it, it just decrements
// the array used size. Clearing the array just sets size = 0.
// Essentially std::array<T> + a count/size.
template<typename T, uint32_t kCapacity>
class FixedSizeArray final : public ArrayBase<T>
{
public:

    using Base = ArrayBase<T>;
    using Base::const_pointer;
    using Base::const_reference;
    using Base::pointer;
    using Base::reference;
    using Base::size_type;
    using Base::value_type;

    FixedSizeArray()
        : Base{ m_array, 0u, kCapacity }
    { }

    FixedSizeArray(const_pointer first, const size_type count)
        : Base{ m_array, count, kCapacity }
    {
        std::copy_n(first, count, m_array);
    }

    FixedSizeArray(const_pointer first, const_pointer last)
        : Base{ m_array, static_cast<size_type>(std::distance(first, last)), kCapacity }
    {
        std::copy(first, last, m_array);
    }

    template<uint32_t kArraySize>
    explicit FixedSizeArray(const value_type (&arr)[kArraySize])
        : Base{ m_array, kArraySize, kCapacity }
    {
        static_assert(kArraySize <= kCapacity);
        std::copy_n(arr, kArraySize, m_array);
    }

private:

    static_assert(kCapacity > 0, "Cannot allocate FixedSizeArray of zero capacity!");
    value_type m_array[kCapacity];
};

} // MrQ2
