#include "app.hpp"
#include "compat/virtual-memory.hpp"
#include "compat/assert.hpp"
#include <utility>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace floormat {

namespace {

#ifdef _WIN32
bool is_committed(const void* p)
{
    MEMORY_BASIC_INFORMATION info;
    fm_assert(VirtualQuery(p, &info, sizeof info) == sizeof info);
    fm_assert(info.State == MEM_COMMIT || info.State == MEM_RESERVE);
    return info.State == MEM_COMMIT;
}
#else
bool is_resident(const void* p)
{
#ifdef __linux__
    unsigned char residency;
#else
    char residency;
#endif
    fm_assert(mincore(const_cast<void*>(p), virtual_memory::page_size(), &residency) == 0);
    return residency & 1;
}
#endif

void test_decommit()
{
    const auto page = virtual_memory::page_size();
    auto m = virtual_memory{};
    m.reserve(4*page);
    m.commit(0, 4*page);
    char* const p = m.data();
    p[0] = 17;
    p[page] = 29;
    p[2*page - 1] = 41;
    p[3*page - 1] = 53;
    p[3*page] = 67;
    m.decommit(page, 2*page);
    m.decommit(0, 0);
#ifdef _WIN32
    fm_assert(is_committed(p) && !is_committed(p + page) && !is_committed(p + 2*page) && is_committed(p + 3*page));
#else
    fm_assert(is_resident(p) && !is_resident(p + page) && !is_resident(p + 2*page) && is_resident(p + 3*page));
#endif
    m.commit(page, 2*page);
    for (usize i = page; i < 3*page; i++)
        fm_assert(p[i] == 0);
    fm_assert(p[0] == 17 && p[3*page] == 67);

    m.decommit(3*page, page);
#ifdef _WIN32
    fm_assert(is_committed(p + 2*page) && !is_committed(p + 3*page));
#endif
    m.commit(3*page, page);
    fm_assert(p[3*page] == 0 && p[0] == 17);
    p[4*page - 1] = 71;
    m.decommit(0, 4*page);
    m.commit(0, 4*page);
    p[page] = 73;
    fm_assert(p[0] == 0 && p[4*page - 1] == 0 && p[page] == 73);
}

} // namespace

void Test::test_virtual_memory()
{
    const auto page = virtual_memory::page_size();
    fm_assert(page >= 4096 && (page & (page - 1)) == 0);

    auto m = virtual_memory{};
    fm_assert(!m.data() && m.size() == 0);
    m.reserve(3*page + 1);
    fm_assert(m.data() && m.size() == 4*page);

    const auto n = m.size();
    m.commit(0, 10);
    m.commit(n - 10, 10);
    char* const p = m.data();
    p[0] = 17;
    p[page - 1] = 29;
    p[n - page] = 41;
    p[n - 1] = 53;
    m.commit(0, 2*page);
    p[2*page - 1] = 67;
    fm_assert(p[0] == 17 && p[page - 1] == 29 && p[2*page - 1] == 67);
    fm_assert(p[n - page] == 41 && p[n - 1] == 53);

    auto m2 = std::move(m);
    fm_assert(!m.data() && m.size() == 0);
    fm_assert(m2.data() == p && m2.size() == n);
    fm_assert(p[n - 1] == 53);

    m2.reserve(page);
    fm_assert(m2.size() == page);
    m2.commit(0, page);
    m2.data()[page - 1] = 79;
    fm_assert(m2.data()[page - 1] == 79);

    test_decommit();
}

} // namespace floormat
