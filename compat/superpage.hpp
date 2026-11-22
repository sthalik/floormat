#pragma once
#include "integer-types.hpp"

namespace floormat {

struct superpage_alloc_t
{
    void* ptr = nullptr;
    u32 size = 0;    // rounded up to large-page or 4 KiB granularity
    bool used_large = false;
};

// Zero-initialized. Never returns null - falls back to plain mmap/VirtualAlloc
// when large pages are unavailable; fm_asserts on OOM.
[[nodiscard]] superpage_alloc_t superpage_alloc(usize bytes) noexcept;
void superpage_free(superpage_alloc_t alloc) noexcept;

#ifdef _WIN32
struct large_pages { usize page; u32 err; };

// err is 0 when the OS reports no large-page support.
large_pages windows_enable_large_pages() noexcept;
#endif

#ifdef __linux__
enum class superpage_mode : u8 { not_supported, always, madvise, never, };

// Returns the kernel's transparent huge page setting for 2 MiB pages, read at the first call,
// or not_supported when PMD pages aren't 2 MiB.
superpage_mode linux_superpage_mode() noexcept;
#endif

} // namespace floormat
