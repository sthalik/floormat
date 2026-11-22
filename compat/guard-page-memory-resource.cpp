#include "guard-page-memory-resource.hpp"
#include "assert.hpp"
#include "atomic.hpp"
#include "strerror.hpp"
#include "virtual-memory.hpp"
#include "random/random.hpp"
#include <cr/StringView.h>
#include <bit>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <sys/mman.h>
#if !defined MAP_ANONYMOUS && defined MAP_ANON
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif
#if fm_ASAN
#include <sanitizer/asan_interface.h>
#endif

namespace floormat {

namespace {

// On 64-bit Windows and x86-64 Linux, mappings end 1 TiB below the top of user space, where libraries go.
#ifdef _WIN64
constexpr uptr min_mapping_address = uptr{1} << 32, max_mapping_address = 0x7effffffffff;
#elif defined __x86_64__ && defined __LP64__ && fm_TSAN
// TSan's mmap() fails or ignores an address outside its application memory, which includes [4 KiB, 512 GiB).
constexpr uptr min_mapping_address = uptr{1} << 32, max_mapping_address = (uptr{1} << 39) - 1;
#elif defined __x86_64__ && defined __LP64__ && defined __FreeBSD__ && fm_ASAN
// ASan's shadow memory on x86-64 FreeBSD ends at 0x500000000000.
constexpr uptr min_mapping_address = 0x500000000000, max_mapping_address = 0x7effffffffff;
#elif defined __x86_64__ && defined __LP64__
// ASan's shadow memory on x86-64 Linux ends at 0x10007fff8000.
constexpr uptr min_mapping_address = 0x10007fff8000, max_mapping_address = 0x7effffffffff;
#elif defined __LP64__
// On AArch64 Linux, ASan's LowMem range is [0, 2^36).
constexpr uptr min_mapping_address = uptr{1} << 32, max_mapping_address = (uptr{1} << 36) - 1;
#else
constexpr uptr min_mapping_address = 0x10000000, max_mapping_address = 0x6fffffff;
#endif

// Each attempt is one mmap() or VirtualAlloc() call at a random address.
// Under ASan on Windows, one in eight addresses lies in ASan's reserved shadow memory.
constexpr u32 max_mapping_attempts = 64;

#ifdef _WIN32
constexpr usize allocation_granularity = 64 * 1024;
#elif defined MAP_FIXED_NOREPLACE
constexpr int fixed_noreplace_flags = MAP_FIXED_NOREPLACE;
#elif defined MAP_EXCL
constexpr int fixed_noreplace_flags = MAP_FIXED | MAP_EXCL;
#else
// The address is a hint. When its range is taken, mmap() returns another address.
constexpr int fixed_noreplace_flags = 0;
#endif

// Every guard_page_memory_resource object takes its addresses from this one sequence.
alignas(8) constinit volatile Random::splitmix64 splitmix64_state;

usize round_up(usize x, usize alignment) { return (x + alignment - 1) & ~(alignment - 1); }

struct mapping_layout
{
    mapping_layout(usize bytes, usize alignment);

    usize mapping_bytes, start_alignment, buffer_offset;
};

mapping_layout::mapping_layout(usize bytes, usize alignment)
{
    fm_assert(std::has_single_bit(alignment) && alignment <= guard_page_memory_resource::max_alignment);
    fm_assert(bytes <= max_mapping_address);
    const usize page = virtual_memory::page_size();
    const usize page_bytes = alignment > page ? alignment : page;
    bytes += !bytes;
    mapping_bytes = round_up(bytes, page_bytes);
#ifdef _WIN32
    start_alignment = page_bytes > allocation_granularity ? page_bytes : allocation_granularity;
#else
    start_alignment = page_bytes;
#endif
    buffer_offset = mapping_bytes - round_up(bytes, alignment);
}

[[noreturn]] void abort_with_os_error(const char* what, usize bytes)
{
    char buf[128];
#ifdef _WIN32
    const auto err = (u32)GetLastError();
    fm_abort("guard_page_memory_resource: %s of %zu bytes failed: %s (GetLastError 0x%08x)",
             what, bytes, get_win32_error_string(buf, err).data(), (unsigned)err);
#else
    fm_abort("guard_page_memory_resource: %s of %zu bytes failed: %s",
             what, bytes, get_error_string(buf, errno).data());
#endif
}

uptr random_address_for_mapping(const mapping_layout& L)
{
    const uptr first = round_up(min_mapping_address, L.start_alignment);
    if (L.mapping_bytes > max_mapping_address - first) [[unlikely]]
        fm_abort("guard_page_memory_resource: %zu bytes don't fit below %p", L.mapping_bytes,
                 (void*)max_mapping_address);
    const uptr num_addresses = (max_mapping_address + 1 - L.mapping_bytes - first) / L.start_alignment + 1;
    return first + (uptr)(Random::next_atomic(splitmix64_state) % num_addresses) * L.start_alignment;
}

char* map_at_random_address(const mapping_layout& L)
{
    for (u32 i = 0; i < max_mapping_attempts; i++)
    {
        void* const addr = (void*)random_address_for_mapping(L);
#ifdef _WIN32
        if (void* const p = VirtualAlloc(addr, L.mapping_bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE))
        {
            fm_assert(p == addr);
            return static_cast<char*>(p);
        }
        else if (GetLastError() != ERROR_INVALID_ADDRESS)
            abort_with_os_error("VirtualAlloc", L.mapping_bytes);
#else
        void* const p = mmap(addr, L.mapping_bytes, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS | fixed_noreplace_flags, -1, 0);
        if (p == addr)
            return static_cast<char*>(p);
        else if (p != MAP_FAILED)
        {
            const int ret = munmap(p, L.mapping_bytes);
            fm_assert(ret == 0);
        }
#if !defined MAP_FIXED_NOREPLACE && defined MAP_EXCL
        // FreeBSD's MAP_EXCL fails with ENOMEM when the range is taken.
        else if (errno != EEXIST && errno != EINVAL && errno != ENOMEM)
#else
        else if (errno != EEXIST && errno != EINVAL)
#endif
            abort_with_os_error("mmap", L.mapping_bytes);
#endif
    }
    fm_abort("guard_page_memory_resource: %u random addresses for %zu bytes were all taken",
             max_mapping_attempts, L.mapping_bytes);
}

} // namespace

guard_page_memory_resource::~guard_page_memory_resource() noexcept
{
    fm_assert(atomic_load(&_num_allocated_buffers, memory_order::relaxed) == 0);
}

u32 guard_page_memory_resource::num_allocated_buffers() const noexcept
{
    return atomic_load(&_num_allocated_buffers, memory_order::relaxed);
}

void* guard_page_memory_resource::allocate(usize bytes, usize alignment) noexcept
{
    const mapping_layout L{bytes, alignment};
    char* const start = map_at_random_address(L);
    char* const p = start + L.buffer_offset;
#if fm_ASAN
    __asan_poison_memory_region(start, L.mapping_bytes);
    __asan_unpoison_memory_region(p, bytes);
#endif
    atomic_fetch_add(&_num_allocated_buffers, 1u, memory_order::relaxed);
    return p;
}

void guard_page_memory_resource::deallocate(void* p, usize bytes, usize alignment) noexcept
{
    const mapping_layout L{bytes, alignment};
    const uptr start = (uptr)p - L.buffer_offset;
    if (start % L.start_alignment != 0) [[unlikely]]
        fm_abort("guard_page_memory_resource: no %zu-byte buffer aligned to %zu starts at %p",
                 bytes, alignment, p);
#ifdef _WIN32
#if fm_ASAN
    // ASan on Windows doesn't intercept VirtualFree, so the pages would stay poisoned after their release.
    __asan_unpoison_memory_region((void*)start, L.mapping_bytes);
#endif
    if (!VirtualFree((void*)start, 0, MEM_RELEASE))
    {
        if (GetLastError() == ERROR_INVALID_ADDRESS)
            fm_abort("guard_page_memory_resource: the %zu-byte buffer at %p was deallocated twice", bytes, p);
        else
            abort_with_os_error("VirtualFree", L.mapping_bytes);
    }
#else
    const usize page = virtual_memory::page_size();
#ifdef __linux__
    unsigned char residency;
#else
    char residency;
#endif
    if (mincore((void*)((uptr)p & ~(uptr)(page - 1)), page, &residency) != 0)
    {
        if (errno == ENOMEM)
            fm_abort("guard_page_memory_resource: the %zu-byte buffer at %p was deallocated twice", bytes, p);
        else
            abort_with_os_error("mincore", page);
    }
    if (munmap((void*)start, L.mapping_bytes) != 0)
        abort_with_os_error("munmap", L.mapping_bytes);
#endif
    const u32 num_buffers_before = atomic_fetch_sub(&_num_allocated_buffers, 1u, memory_order::relaxed);
    fm_assert(num_buffers_before > 0);
}

} // namespace floormat
