//
// Memory.cpp
//  Memory tags for budget tracking and other custom memory allocation helpers.
//

#include "Memory.hpp"
#include "Common.hpp"

#include <cstdlib> // malloc/free
#include <cstring> // memset/cpy
#include <cstdio>

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////

// Verbose debugging
constexpr bool kLogNewDeleteCalls = false;
constexpr bool kHunkAllocVerbose  = false;

static const char * const MemTag_Strings[] = {
    "kGame",
    "kRenderer",
    "kTextures",
    "kWorldModel",
    "kAliasModel",
    "kSpriteModel",
    "kVertIndexBuffer",
};

static_assert(ArrayLength(MemTag_Strings) == unsigned(MemTag::kCount), "Update this if the enum changes!");

///////////////////////////////////////////////////////////////////////////////
// MemTags
///////////////////////////////////////////////////////////////////////////////

struct MemCounts
{
    std::size_t total_bytes;
    std::size_t total_allocs;
    std::size_t total_frees;
    std::size_t smallest_alloc;
    std::size_t largest_alloc;
};

// Current allocation counts for each memory tag
static MemCounts MemTag_Counts[unsigned(MemTag::kCount)];

///////////////////////////////////////////////////////////////////////////////

void MemTagsTrackAlloc(const std::size_t size_bytes, const MemTag tag)
{
    const auto idx = unsigned(tag);
    MRQ2_ASSERT(idx < unsigned(MemTag::kCount));

    MemTag_Counts[idx].total_bytes += size_bytes;
    MemTag_Counts[idx].total_allocs++;

    if ((size_bytes < MemTag_Counts[idx].smallest_alloc) || (MemTag_Counts[idx].smallest_alloc == 0))
    {
        MemTag_Counts[idx].smallest_alloc = size_bytes;
    }
    if (size_bytes > MemTag_Counts[idx].largest_alloc)
    {
        MemTag_Counts[idx].largest_alloc = size_bytes;
    }
}

///////////////////////////////////////////////////////////////////////////////

void MemTagsTrackFree(const std::size_t size_bytes, const MemTag tag)
{
    const auto idx = unsigned(tag);
    MRQ2_ASSERT(idx < unsigned(MemTag::kCount));

    MemTag_Counts[idx].total_bytes -= size_bytes;
    MemTag_Counts[idx].total_frees++;
}

///////////////////////////////////////////////////////////////////////////////

void MemTagsClearAll()
{
    std::memset(MemTag_Counts, 0, sizeof(MemTag_Counts));
}

///////////////////////////////////////////////////////////////////////////////

void MemTagsPrintAll()
{
#ifdef _MSC_VER
    #pragma warning(disable: 4996) // "sprintf: This function or variable may be unsafe"
#endif // _MSC_VER

    char tags_dump_str[4096] = {'\0'};
    char * ptr = tags_dump_str;
    std::size_t mem_total = 0;

    ptr += std::sprintf(ptr, "--------------------------- MEMTAGS ---------------------------\n");
    ptr += std::sprintf(ptr, "Tag Name          Bytes      Allocs  Frees   Small    Large\n");

    for (int i = 0; i < int(MemTag::kCount); ++i)
    {
        mem_total += MemTag_Counts[i].total_bytes;
        const char * const total_str = FormatMemoryUnit(MemTag_Counts[i].total_bytes);

        ptr += std::sprintf(ptr, "%-17s %-10s %-7zu %-7zu %-8zu %-8zu\n",
                            MemTag_Strings[i], total_str,
                            MemTag_Counts[i].total_allocs,
                            MemTag_Counts[i].total_frees,
                            MemTag_Counts[i].smallest_alloc,
                            MemTag_Counts[i].largest_alloc);
    }

    ptr += std::sprintf(ptr, "\nTOTAL MEM: %s\n", FormatMemoryUnit(mem_total));
    ptr += std::sprintf(ptr, "--------------------------- MEMTAGS ---------------------------");

    GameInterface::Printf("\n%s\n", tags_dump_str);
}

///////////////////////////////////////////////////////////////////////////////

const char * FormatMemoryUnit(const std::size_t size_bytes, const bool abbreviated)
{
    enum {
        KILOBYTE = 1024,
        MEGABYTE = 1024 * KILOBYTE,
        GIGABYTE = 1024 * MEGABYTE
    };

    const char * mem_unit_str;
    float adjusted_size;

    if (size_bytes < KILOBYTE)
    {
        mem_unit_str = abbreviated ? "B" : "Bytes";
        adjusted_size = static_cast<float>(size_bytes); // Might overflow if really big...
    }
    else if (size_bytes < MEGABYTE)
    {
        mem_unit_str = abbreviated ? "KB" : "Kilobytes";
        adjusted_size = size_bytes / 1024.0f;
    }
    else if (size_bytes < GIGABYTE)
    {
        mem_unit_str = abbreviated ? "MB" : "Megabytes";
        adjusted_size = size_bytes / 1024.0f / 1024.0f;
    }
    else
    {
        mem_unit_str = abbreviated ? "GB" : "Gigabytes";
        adjusted_size = size_bytes / 1024.0f / 1024.0f / 1024.0f;
    }

    char * fmtbuf;
    char num_str_buf[100];

    // Max chars reserved for the output string, max `num_str_buf + 28` chars for the unit str
    constexpr unsigned kMemUnitStrMaxLen = sizeof(num_str_buf) + 28;

    static int bufnum = 0;
    static char s_local_str_buf[8][kMemUnitStrMaxLen];

    fmtbuf = s_local_str_buf[bufnum];
    bufnum = (bufnum + 1) & 7;

    // Only care about the first 2 decimal digits
    std::snprintf(num_str_buf, sizeof(num_str_buf), "%.2f", adjusted_size);

    // Remove trailing zeros if no significant decimal digits:
    char * p = num_str_buf;
    for (; *p; ++p)
    {
        if (*p == '.')
        {
            // Find the end of the string:
            while (*++p)
            {
            }
            // Remove trailing zeros:
            while (*--p == '0')
            {
                *p = '\0';
            }
            // If the dot was left alone at the end, remove it too.
            if (*p == '.')
            {
                *p = '\0';
            }
            break;
        }
    }

    // Combine the strings:
    std::snprintf(fmtbuf, kMemUnitStrMaxLen, "%s %s", num_str_buf, mem_unit_str);
    return fmtbuf;
}

///////////////////////////////////////////////////////////////////////////////

void * MemAllocTracked(const size_t size_bytes, const MemTag tag)
{
    MemTagsTrackAlloc(size_bytes, tag);
    return std::malloc(size_bytes);
}

///////////////////////////////////////////////////////////////////////////////

void MemFreeTracked(const void * ptr, const size_t size_bytes, const MemTag tag)
{
    if (kLogNewDeleteCalls)
    {
        GameInterface::Printf("MemFreeTracked(0x%p, %zu, %s)", ptr, size_bytes, MemTag_Strings[unsigned(tag)]);
    }

    MemTagsTrackFree(size_bytes, tag);
    std::free((void *)ptr);
}

///////////////////////////////////////////////////////////////////////////////
// MemHunk
///////////////////////////////////////////////////////////////////////////////

constexpr unsigned kHunkSizeRound = 31;

///////////////////////////////////////////////////////////////////////////////

MemHunk::~MemHunk()
{
    if (base_ptr != nullptr)
    {
        MemFreeTracked(base_ptr, max_size, mem_tag);
        base_ptr = nullptr;
    }
}

///////////////////////////////////////////////////////////////////////////////

void MemHunk::Init(const unsigned size, const MemTag tag)
{
    MRQ2_ASSERT(base_ptr == nullptr); // Trap invalid reinitialization

    const unsigned rounded_size = (size + kHunkSizeRound) & ~kHunkSizeRound;
    MRQ2_ASSERT(rounded_size >= size);

    curr_size = 0;
    max_size  = rounded_size;
    mem_tag   = tag;
    base_ptr  = new(tag) std::uint8_t[rounded_size];
    std::memset(base_ptr, 0, rounded_size);

    if (kHunkAllocVerbose)
    {
        GameInterface::Printf("MemHunk::Init(%s, %s)", FormatMemoryUnit(rounded_size), MemTag_Strings[unsigned(tag)]);
    }
}

///////////////////////////////////////////////////////////////////////////////

void * MemHunk::AllocBlock(const unsigned block_size)
{
    MRQ2_ASSERT(base_ptr != nullptr); // Uninitialized?

    const unsigned rounded_size = (block_size + kHunkSizeRound) & ~kHunkSizeRound;
    MRQ2_ASSERT(rounded_size >= block_size);

    // The hunk stack doesn't resize.
    curr_size += rounded_size;
    if (curr_size > max_size)
    {
        GameInterface::Errorf("MemHunk::AllocBlock: Overflowed with %u bytes request!", rounded_size);
    }

    if (kHunkAllocVerbose)
    {
        GameInterface::Printf("MemHunk::AllocBlock(%s) -> left %s",
                              FormatMemoryUnit(rounded_size),
                              FormatMemoryUnit(max_size - curr_size));
    }

    return base_ptr + curr_size - rounded_size;
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2

///////////////////////////////////////////////////////////////////////////////
// Global new/delete operator overrides
///////////////////////////////////////////////////////////////////////////////

void * operator new(const std::size_t size_bytes, const MrQ2::MemTag tag)
{
    if (MrQ2::kLogNewDeleteCalls)
    {
        MrQ2::GameInterface::Printf("operator new(%zu, %s)", size_bytes, MrQ2::MemTag_Strings[unsigned(tag)]);
    }
    return MrQ2::MemAllocTracked(size_bytes, tag);
}

///////////////////////////////////////////////////////////////////////////////

void * operator new[](const std::size_t size_bytes, const MrQ2::MemTag tag)
{
    if (MrQ2::kLogNewDeleteCalls)
    {
        MrQ2::GameInterface::Printf("operator new[](%zu, %s)", size_bytes, MrQ2::MemTag_Strings[unsigned(tag)]);
    }
    return MrQ2::MemAllocTracked(size_bytes, tag);
}

///////////////////////////////////////////////////////////////////////////////
