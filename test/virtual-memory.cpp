#include "app.hpp"
#include "compat/virtual-memory.hpp"
#include "compat/assert.hpp"
#include <utility>

namespace floormat {

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
}

} // namespace floormat
