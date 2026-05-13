#pragma once

namespace floormat {

struct superpage_alloc_t
{
    void* ptr = nullptr;
    size_t size = 0;      // rounded up to large-page or 4 KiB granularity
    bool used_large = false;
};

// Zero-initialized. Never returns null - falls back to plain mmap/VirtualAlloc
// when large pages are unavailable; fm_asserts on OOM.
[[nodiscard]] superpage_alloc_t superpage_alloc(size_t bytes) noexcept;
void superpage_free(superpage_alloc_t alloc) noexcept;

} // namespace floormat
