//
// Pool.hpp
//  Simple block-based growable memory pool.
//
#pragma once

#include "Memory.hpp"
#include <cstring>
#include <memory>
#include <utility>

namespace MrQ2
{

//
// Pool of fixed-size memory blocks (similar to a list of arrays).
//
// This pool allocator operates as a linked list of small arrays.
// Each array is a pool of blocks with the size of 'T' template parameter.
// Template parameter 'kGranularity' defines the size in objects of type 'T'
// of such arrays.
//
// Allocate() will return an uninitialized memory block.
// The user is responsible for calling Construct() on it to run class
// constructors if necessary, and Destroy() to call class destructor
// before deallocating the block.
//
template<typename T, int kGranularity>
class Pool final
{
public:

    // Empty pool; no allocation until first use.
    explicit Pool(MemTag tag);

    // Drains the pool.
    ~Pool();

    // Not copyable.
    Pool(const Pool &) = delete;
    Pool & operator=(const Pool &) = delete;

    // Allocates a single memory block of size 'T' and
    // returns an uninitialized pointer to it.
    T * Allocate();

    // Deallocates a memory block previously allocated by 'Allocate()'.
    // Pointer may be null, in which case this is a no-op. NOTE: Class destructor NOT called!
    void Deallocate(void * ptr);

    // Frees all blocks, reseting the pool allocator to its initial state.
    // WARNING: Calling this method will invalidate any memory block still
    // alive that was previously allocated from this pool.
    void Drain();

    // Miscellaneous pool stats:
    int TotalAllocs()  const;
    int TotalFrees()   const;
    int ObjectsAlive() const;
    int BlockCount()   const;

    constexpr static std::size_t PoolGranularity();
    constexpr static std::size_t PooledObjectSize();

private:

    // Fill patterns for debug allocations.
    #if defined(DEBUG) || defined(_DEBUG)
    static constexpr std::uint8_t kAllocFillVal = 0xCD; // 'Clean memory' -> New allocation
    static constexpr std::uint8_t kFreeFillVal  = 0xDD; // 'Dead memory'  -> Freed/deleted
    #endif // DEBUG

    union PoolObj
    {
        alignas(T) std::uint8_t bytes[sizeof(T)];
        PoolObj * next;
    };

    struct PoolBlock
    {
        PoolObj objects[kGranularity];
        PoolBlock * next;
    };

    PoolBlock * m_block_list = nullptr; // List of all blocks/pools.
    PoolObj   * m_free_list  = nullptr; // List of free objects that can be recycled.

    int m_alloc_count        = 0;       // Total calls to 'Allocate()'.
    int m_object_count       = 0;       // User objects ('T' instances) currently alive.
    int m_pool_block_count   = 0;       // Size in blocks of the 'm_block_list'.
    const MemTag m_mem_tag;             // Memory tag for allocations done by the pool.
};

///////////////////////////////////////////////////////////////////////////////
// Pool inline implementation:
///////////////////////////////////////////////////////////////////////////////

template<typename T, int kGranularity>
inline Pool<T, kGranularity>::Pool(const MemTag tag)
    : m_mem_tag{ tag }
{
}

template<typename T, int kGranularity>
inline Pool<T, kGranularity>::~Pool()
{
    Drain();
}

template<typename T, int kGranularity>
inline T * Pool<T, kGranularity>::Allocate()
{
    if (m_free_list == nullptr)
    {
        auto newBlock = new(m_mem_tag) PoolBlock{};
        newBlock->next = m_block_list;
        m_block_list = newBlock;

        ++m_pool_block_count;

        // All objects in the new pool block are appended
        // to the free list, since they are ready to be used.
        for (int i = 0; i < kGranularity; ++i)
        {
            newBlock->objects[i].next = m_free_list;
            m_free_list = &newBlock->objects[i];
        }
    }

    ++m_alloc_count;
    ++m_object_count;

    // Fetch one from the free list's head:
    PoolObj * object = m_free_list;
    m_free_list = m_free_list->next;

    // Initializing the object with a known pattern
    // to help detecting memory errors.
    #if defined(DEBUG) || defined(_DEBUG)
    std::memset(object, kAllocFillVal, sizeof(PoolObj));
    #endif // DEBUG

    return reinterpret_cast<T *>(object);
}

template<typename T, int kGranularity>
inline void Pool<T, kGranularity>::Deallocate(void * ptr)
{
    #if defined(DEBUG) || defined(_DEBUG)
    if (m_object_count <= 0) // Can't deallocate from an empty pool
    {
        std::abort();
    }
    #endif // DEBUG

    if (ptr == nullptr)
    {
        return;
    }

    // Fill user portion with a known pattern to help
    // detecting post-deallocation usage attempts.
    #if defined(DEBUG) || defined(_DEBUG)
    std::memset(ptr, kFreeFillVal, sizeof(PoolObj));
    #endif // DEBUG

    // Add back to free list's head. Memory not actually freed now.
    auto object = static_cast<PoolObj *>(ptr);
    object->next = m_free_list;
    m_free_list = object;

    --m_object_count;
}

template<typename T, int kGranularity>
inline void Pool<T, kGranularity>::Drain()
{
    while (m_block_list != nullptr)
    {
        PoolBlock * block = m_block_list;
        m_block_list = m_block_list->next;
        DeleteObject(block, m_mem_tag);
    }

    m_block_list       = nullptr;
    m_free_list        = nullptr;
    m_alloc_count      = 0;
    m_object_count     = 0;
    m_pool_block_count = 0;
}

template<typename T, int kGranularity>
inline int Pool<T, kGranularity>::TotalAllocs() const
{
    return m_alloc_count;
}

template<typename T, int kGranularity>
inline int Pool<T, kGranularity>::TotalFrees() const
{
    return m_alloc_count - m_object_count;
}

template<typename T, int kGranularity>
inline int Pool<T, kGranularity>::ObjectsAlive() const
{
    return m_object_count;
}

template<typename T, int kGranularity>
inline int Pool<T, kGranularity>::BlockCount() const
{
    return m_pool_block_count;
}

template<typename T, int kGranularity>
constexpr std::size_t Pool<T, kGranularity>::PoolGranularity()
{
    return kGranularity;
}

template<typename T, int kGranularity>
constexpr std::size_t Pool<T, kGranularity>::PooledObjectSize()
{
    return sizeof(T);
}

///////////////////////////////////////////////////////////////////////////////
// Construct() / Destroy() helpers:
///////////////////////////////////////////////////////////////////////////////

template<typename T>
inline T * Construct(T * obj, const T & other) // Copy constructor
{
    return ::new(obj) T(other);
}

template<typename T, typename... Args>
inline T * Construct(T * obj, Args &&... args) // Parameterized or default constructor
{
    return ::new(obj) T(std::forward<Args>(args)...);
}

template<typename T>
inline void Destroy(T * obj)
{
    if (obj != nullptr)
    {
        obj->~T();
    }
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
