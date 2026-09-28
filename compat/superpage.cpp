#include "superpage.hpp"
#include "assert.hpp"
#include <atomic>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#  ifdef __FreeBSD__
#    include <fcntl.h>
#  endif
#  ifdef __APPLE__
#    include <mach/mach.h>
#    include <mach/mach_vm.h>
#    include <mach/vm_statistics.h>
#  endif
#endif

#if !defined MAP_ANONYMOUS && defined MAP_ANON
#define MAP_ANONYMOUS MAP_ANON
#endif

namespace floormat {

namespace {

// Circuit breaker: after the first failure, skip the large-page path entirely.
// SeLockMemoryPrivilege missing, hugepage pool empty, etc. don't change mid-run.
std::atomic large_failed{false};

[[maybe_unused]] constexpr size_t LARGE_PAGE_FALLBACK = 2u << 20;   // 2 MiB - typical x86_64 large page
[[maybe_unused]] constexpr size_t SMALL_PAGE_FALLBACK = 4u << 10;   // 4 KiB - last-resort rounding

inline size_t round_up(size_t bytes, size_t page) noexcept
{
    return (bytes + page - 1) & ~(page - 1);
}

#ifdef _WIN32
size_t windows_enable_large_pages() noexcept
{
    static const size_t result = []() -> size_t {
        size_t page = GetLargePageMinimum();
        if (page == 0)
            return 0;
        HANDLE tok = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(),
                              TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok))
            return 0;
        TOKEN_PRIVILEGES tp = {};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        BOOL ok = LookupPrivilegeValueA(nullptr, "SeLockMemoryPrivilege",
                                        &tp.Privileges[0].Luid);
        if (ok)
            ok = AdjustTokenPrivileges(tok, FALSE, &tp, sizeof(tp), nullptr, nullptr);
        DWORD err = GetLastError();
        CloseHandle(tok);
        if (!ok || err == ERROR_NOT_ALL_ASSIGNED)
            return 0;
        return page;
    }();
    return result;
}
#endif

#if defined __FreeBSD__ && defined SHM_LARGEPAGE_ALLOC_DEFAULT
int freebsd_psind_2m() noexcept
{
    static const int result = [] {
        size_t sizes[8];
        int n = getpagesizes(sizes, 8);
        for (int i = 1; i < n; i++)
            if (sizes[i] == LARGE_PAGE_FALLBACK)
                return i;
        return 0;
    }();
    return result;
}
#endif

} // namespace

superpage_alloc_t superpage_alloc(size_t bytes) noexcept
{
    fm_assert(bytes > 0);

#ifdef _WIN32
    if (!large_failed.load(std::memory_order_relaxed))
    {
        if (size_t page = windows_enable_large_pages(); page > 0)
        {
            size_t sz = round_up(bytes, page);
            void* p = VirtualAlloc(nullptr, sz,
                                   MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES,
                                   PAGE_READWRITE);
            if (p)
                return { p, sz, true };
        }
        large_failed.store(true, std::memory_order_relaxed);
    }
    size_t sz = round_up(bytes, SMALL_PAGE_FALLBACK);
    void* p = VirtualAlloc(nullptr, sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    fm_assert(p);
    return { p, sz, false };

#else
    size_t page = (size_t)sysconf(_SC_PAGESIZE);
    if (page == 0)
        page = SMALL_PAGE_FALLBACK;

#  if defined __linux__ && defined MAP_HUGETLB
    if (!large_failed.load(std::memory_order_relaxed))
    {
        size_t sz = round_up(bytes, LARGE_PAGE_FALLBACK);
        int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB;
#    ifdef MAP_HUGE_2MB
        flags |= MAP_HUGE_2MB;
#    endif
        void* p = mmap(nullptr, sz, PROT_READ | PROT_WRITE, flags, -1, 0);
        if (p != MAP_FAILED)
            return { p, sz, true };
        large_failed.store(true, std::memory_order_relaxed);
    }
    size_t sz = round_up(bytes, page);
    void* p = mmap(nullptr, sz, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    fm_assert(p != MAP_FAILED);
#    ifdef MADV_HUGEPAGE
    // Opportunistic THP promotion; ignored if kernel declines.
    (void)madvise(p, sz, MADV_HUGEPAGE);
#    endif
    return { p, sz, false };

#  elif defined __FreeBSD__ && defined MAP_ALIGNED_SUPER
#    ifdef SHM_LARGEPAGE_ALLOC_DEFAULT
    if (!large_failed.load(std::memory_order_relaxed))
    {
        if (int psind = freebsd_psind_2m(); psind > 0)
        {
            size_t sz = round_up(bytes, LARGE_PAGE_FALLBACK);
            // HARD can defragment forever. NOWAIT fails as soon as memory is fragmented.
            int fd = shm_create_largepage(SHM_ANON, O_RDWR, psind, SHM_LARGEPAGE_ALLOC_DEFAULT, 0);
            if (fd >= 0)
            {
                void* p = MAP_FAILED;
                if (ftruncate(fd, (off_t)sz) == 0)
                    p = mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                close(fd);
                if (p != MAP_FAILED)
                    return { p, sz, true };
            }
        }
        large_failed.store(true, std::memory_order_relaxed);
    }
#    endif
    // MAP_ALIGNED_SUPER gets superpages only if a free 2 MiB block exists.
    size_t sz = round_up(bytes, page);
    void* p = mmap(nullptr, sz, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_ALIGNED_SUPER, -1, 0);
    if (p == MAP_FAILED)
        p = mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    fm_assert(p != MAP_FAILED);
    return { p, sz, false };

#  elif defined __APPLE__
    // Intel macOS: VM_FLAGS_SUPERPAGE_SIZE_2MB is defined only on x86_64 SDKs
    // and some macOS releases require an entitlement; circuit-break on failure.
#    ifdef VM_FLAGS_SUPERPAGE_SIZE_2MB
    if (!large_failed.load(std::memory_order_relaxed))
    {
        size_t large_sz = round_up(bytes, LARGE_PAGE_FALLBACK);
        mach_vm_address_t addr = 0;
        kern_return_t kr = mach_vm_allocate(
            mach_task_self(), &addr, large_sz,
            VM_FLAGS_ANYWHERE | VM_FLAGS_SUPERPAGE_SIZE_2MB);
        if (kr == KERN_SUCCESS)
            return { reinterpret_cast<void*>(addr), large_sz, true };
        large_failed.store(true, std::memory_order_relaxed);
    }
#    endif
    size_t sz = round_up(bytes, page);
    void* p = mmap(nullptr, sz, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    fm_assert(p != MAP_FAILED);
    return { p, sz, false };

#  else
    // generic POSIX: plain anonymous mmap, zero-page-backed on demand.
    size_t sz = round_up(bytes, page);
    void* p = mmap(nullptr, sz, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    fm_assert(p != MAP_FAILED);
    return { p, sz, false };
#  endif
#endif
}

void superpage_free(superpage_alloc_t a) noexcept
{
    if (!a.ptr)
        return;
#ifdef _WIN32
    BOOL ok = VirtualFree(a.ptr, 0, MEM_RELEASE);
    fm_assert(ok);
#elif defined __APPLE__ && defined VM_FLAGS_SUPERPAGE_SIZE_2MB
    // macOS large path goes through mach_vm_allocate, plain path through mmap.
    bool ok;
    if (a.used_large)
        ok = mach_vm_deallocate(mach_task_self(), reinterpret_cast<mach_vm_address_t>(a.ptr), a.size) == KERN_SUCCESS;
    else
        ok = munmap(a.ptr, a.size) == 0;
    fm_assert(ok);
#else
    int r = munmap(a.ptr, a.size);
    fm_assert(r == 0);
#endif
}

} // namespace floormat
