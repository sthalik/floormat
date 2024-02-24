#include "app.hpp"
#include "src/world.hpp"
#include "src/chunk-region.hpp"
#include "loader/loader.hpp"

namespace floormat {

namespace {

chunk& make_chunk1(chunk& c, bool val, bool flipped)
{
    auto floor = tile_image_proto { loader.ground_atlas("texel"), 0 };
    auto empty = tile_image_proto{};

    constexpr uint8_t mat[TILE_MAX_DIM][TILE_MAX_DIM] = {
        { 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        { 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, },
        { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    };

    if (!flipped)
        for (auto j = 0u; j < TILE_MAX_DIM; j++)
            for (auto i = 0u; i < TILE_MAX_DIM; i++)
                c[{i, j}].ground() = !!mat[i][j] == val ? floor : empty;
    else
        for (auto j = 0u; j < TILE_MAX_DIM; j++)
            for (auto i = 0u; i < TILE_MAX_DIM; i++)
                c[{i, j}].ground() = !!mat[j][i] == val ? floor : empty;

    return c;
}

constexpr auto COORD = chunk_coords_{2, 1, 0};

void test1()
{
    auto w = world();
    auto& c = make_chunk1(w[COORD], true, false);
    auto p = chunk::pass_region{};
    c.make_pass_region(p);
    const auto count = p.bits.count();
    fm_assert(count > 1000);
    fm_assert(p.bits.size() - count > 2000);
    fm_assert(c.get_pass_region()->bits == p.bits);
    { auto w = world();
      auto& c = make_chunk1(w[COORD], true, true);
      auto p2 = chunk::pass_region{};
      c.make_pass_region(p2);
      fm_assert(p2.bits.count() == count);
    }
}

void test2()
{
    auto w = world();
    auto& c = make_chunk1(w[COORD], false, false);
    auto p = chunk::pass_region{};
    c.make_pass_region(p);
    const auto count = p.bits.count();
    fm_assert(count > 1000);
    fm_assert(p.bits.size() - count > 2000);
    { auto w = world();
      auto& c = make_chunk1(w[COORD], false, true);
      auto p2 = chunk::pass_region{};
      c.make_pass_region(p2);
      fm_assert(p2.bits.count() == count);
    }
}

void test3()
{
    auto w = world();
    auto& c = make_chunk1(w[COORD], true, false);
    const auto bits = c.get_pass_region()->bits;
    fm_assert(bits.count() > 1000);

    { auto p2 = chunk::pass_region{};
      c.make_pass_region(p2);
      fm_assert(p2.bits == bits);
    }

    { const auto empty = tile_image_proto{};
      fm_assert(c[{0, 0}].ground() != empty);

      c[{0, 0}].ground() = empty;
      fm_assert(c.get_pass_region()->bits == bits);

      c.mark_passability_modified();
      fm_assert(c.get_pass_region()->bits != bits);
    }
}

} // namespace

void test_app::test_region()
{
    test1();
    test2();
    test3();
}

} // namespace floormat
