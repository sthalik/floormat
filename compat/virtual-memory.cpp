#include "virtual-memory.hpp"
#include "assert.hpp"
#include "strerror.hpp"
#include <cr/StringView.h>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <unistd.h>
#include <sys/mman.h>
#if !defined MAP_ANONYMOUS && defined MAP_ANON
#define MAP_ANONYMOUS MAP_ANON
#endif
#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif
#endif

namespace floormat {

namespace {

usize round_up(usize bytes, usize page) { return (bytes + page - 1) & ~(page - 1); }

[[noreturn]] void abort_with_os_error(const char* what, usize bytes)
{
    char buf[128];
#ifdef _WIN32
    const auto err = (u32)GetLastError();
    fm_abort("virtual_memory: %s of %zu bytes failed: %s (GetLastError 0x%08x)",
             what, bytes, get_win32_error_string(buf, err).data(), (unsigned)err);
#else
    fm_abort("virtual_memory: %s of %zu bytes failed: %s", what, bytes, get_error_string(buf, errno).data());
#endif
}

} // namespace

usize virtual_memory::page_size()
{
    static const usize ret = [] {
#ifdef _WIN32
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return (usize)si.dwPageSize;
#else
        return (usize)sysconf(_SC_PAGESIZE);
#endif
    }();
    return ret;
}

virtual_memory::virtual_memory() noexcept = default;

virtual_memory::~virtual_memory() noexcept { release(); }

virtual_memory::virtual_memory(virtual_memory&& other) noexcept:
    _data{std::exchange(other._data, nullptr)},
    _size{std::exchange(other._size, 0)}
{
}

virtual_memory& virtual_memory::operator=(virtual_memory&& other) noexcept
{
    if (this != &other)
    {
        release();
        _data = std::exchange(other._data, nullptr);
        _size = std::exchange(other._size, 0);
    }
    return *this;
}

void virtual_memory::release() noexcept
{
    if (!_data)
        return;
#ifdef _WIN32
    const bool ok = VirtualFree(_data, 0, MEM_RELEASE);
#else
    const bool ok = munmap(_data, _size) == 0;
#endif
    if (!ok)
        abort_with_os_error("release", _size);
    _data = nullptr;
    _size = 0;
}

void virtual_memory::reserve(usize bytes)
{
    fm_assert(bytes > 0);
    release();
    const auto size = round_up(bytes, page_size());
#ifdef _WIN32
    auto* p = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);
    if (!p)
        abort_with_os_error("reserve", size);
#else
    auto* p = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (p == MAP_FAILED)
        abort_with_os_error("reserve", size);
#endif
    _data = (char*)p;
    _size = size;
}

void virtual_memory::commit(usize offset, usize bytes)
{
    fm_assert(offset <= _size && bytes <= _size - offset);
    if (bytes == 0)
        return;
    const auto page = page_size();
    const auto start = offset & ~(page - 1), end = round_up(offset + bytes, page);
#ifdef _WIN32
    if (!VirtualAlloc(_data + start, end - start, MEM_COMMIT, PAGE_READWRITE))
        abort_with_os_error("commit", end - start);
#else
    if (mprotect(_data + start, end - start, PROT_READ | PROT_WRITE) != 0)
        abort_with_os_error("commit", end - start);
#endif
}

} // namespace floormat
