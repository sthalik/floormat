#include "app.hpp"
#include "src/chunk.hpp"
#include "src/chunk-iter.hpp"
#include "src/world.hpp"
#include "src/object.hpp"
#include "compat/borrowed-ptr.inl"

namespace floormat {

namespace {

void test_empty()
{
    world w;
    chunk_coords_ ch{0, 0, 0};
    chunk& c = w[ch];
    const chunk& cc = c;

    auto view = cc.objects();
    fm_assert(view.size() == 0);
    fm_assert(view.begin() == view.end());

    size_t n = 0;
    for ([[maybe_unused]] const object& o : view)
        ++n;
    fm_assert(n == 0);
}

void test_populated()
{
    world w;
    chunk_coords_ ch{0, 0, 0};
    chunk& c = Test::make_test_chunk(w, ch);
    c.sort_objects();

    auto mut = c.objects();
    fm_assert(mut.size() > 0);

    const chunk& cc = c;
    auto view = cc.objects();
    fm_assert(view.size() == mut.size());

    size_t i = 0;
    for (const object& o : view)
    {
        fm_assert(&o == &*mut[i]);
        ++i;
    }
    fm_assert(i == view.size());

    for (size_t k = 0; k < view.size(); ++k)
        fm_assert(&view[k] == &*mut[k]);

    auto it = view.begin();
    auto end = view.end();
    size_t walked = 0;
    while (it != end)
    {
        ++it;
        ++walked;
    }
    fm_assert(walked == view.size());
}

} // namespace

void Test::test_chunk_iter()
{
    test_empty();
    test_populated();
}

} // namespace floormat
