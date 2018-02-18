//
// Memory.hpp
//  Memory tags for budget tracking and other custom memory allocation helpers.
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

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

/*
===============================================================================

    Global new/delete operator overrides

===============================================================================
*/

void * operator new(std::size_t size_bytes, MemTag tag);
void * operator new[](std::size_t size_bytes, MemTag tag);
void MemFreeTracked(const void * ptr, size_t size_bytes, MemTag tag);

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
    std::size_t    max_size  = 0;
    std::size_t    curr_size = 0;
    MemTag         mem_tag;

    // Destructor frees the whole memory hunk.
    MemHunk() = default;
    ~MemHunk();

    // Allocate a new hunk of memory (allocation is zero filled).
    void Init(std::size_t size, MemTag tag);

    // Fetch a new slice from the hunk's end.
    void * AllocBlock(std::size_t block_size);

    // Get the offset to the end of the allocated region.
    std::size_t Tail() const { return curr_size; }
};

// ============================================================================
