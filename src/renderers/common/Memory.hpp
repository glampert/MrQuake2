//
// Memory.hpp
//  Memory tags for budget tracking and other custom memory allocation helpers.
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace MrQ2
{

/*
===============================================================================

    Memory allocation tags for tracking

===============================================================================
*/
enum class MemTag : std::uint8_t
{
    kGame = 0, // G_MEMTAG_ZTAGALLOC in q_shared.h

    // Tags from the Ref lib
    kRenderer,
    kTextures,
    kLightmaps,
    kWorldModel,
    kAliasModel,
    kSpriteModel,
    kVertIndexBuffer,

    // Number of items in the enum - not a valid mem tag.
    kCount
};

// Increase/decrease memory usage for the given tag on allocs/frees.
void MemTagsTrackAlloc(std::size_t size_bytes, MemTag tag);
void MemTagsTrackFree(std::size_t size_bytes, MemTag tag);

// Reset all tags to zero / dump them to the console.
void MemTagsClearAll();
void MemTagsPrintAll();

// Convenient helper to print a memory size into a string using the shortest representation for the size.
// Note: Returns a pointer to a static buffer - copy if you need to hold on to it.
const char * FormatMemoryUnit(std::size_t size_bytes, bool abbreviated = true);

// Internal memory allocation/deallocation functions with tag tracking.
void * MemAllocTracked(size_t size_bytes, MemTag tag);
void MemFreeTracked(const void * ptr, size_t size_bytes, MemTag tag);

// Destroys a single object pointer and frees it - passes along the tag for tracking.
template<typename T>
inline void DeleteObject(T * const obj, const MemTag tag)
{
    if (obj == nullptr) return;
    if (!std::is_trivially_destructible<T>::value)
    {
        obj->~T();
    }
    MemFreeTracked(obj, sizeof(T), tag);
}

// Destroys a array of objects and frees it - passes along the tag for tracking.
template<typename T>
inline void DeleteArray(T * const first, const std::size_t count, const MemTag tag)
{
    if (first == nullptr) return;
    if (!std::is_trivially_destructible<T>::value)
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            first[i].~T();
        }
    }
    MemFreeTracked(first, sizeof(T) * count, tag);
}

/*
===============================================================================

    Large block linear allocation API (the "Hunk" allocator from Quake)

===============================================================================
*/
struct MemHunk final
{
    std::uint8_t * base_ptr  = nullptr;
    unsigned       max_size  = 0;
    unsigned       curr_size = 0;
    MemTag         mem_tag;

    // Destructor frees the whole memory hunk.
    MemHunk() = default;
    ~MemHunk();

    // Allocate a new hunk of memory (allocation is zero filled).
    void Init(unsigned size, MemTag tag);

    // Fetch a new slice from the hunk's end.
    void * AllocBlock(unsigned block_size);

    // Allocate 'count' instances of struct/type T at the hunk's end.
    template<typename T> T * AllocBlockOfType(unsigned count)
    {
        return static_cast<T *>(AllocBlock(count * sizeof(T)));
    }

    // Get pointer to start of the hunk with cast to the given type.
    template<typename T> T * ViewBaseAs() const
    {
        return reinterpret_cast<T *>(base_ptr);
    }

    // Get the offset to the end of the allocated region.
    unsigned Tail() const { return curr_size; }
};

} // MrQ2

/*
===============================================================================

    Global new/delete operator overrides

===============================================================================
*/
void * operator new(std::size_t size_bytes, MrQ2::MemTag tag);
void * operator new[](std::size_t size_bytes, MrQ2::MemTag tag);
