#include "chunk-region.hpp"
#include "search-bbox.hpp"
#include "world.hpp"
#include "collision.hpp"
#include "object.hpp"
#include "compat/debug.hpp"
#include "compat/function2.hpp"
#include <bit>
#include <array>
#include <Corrade/Containers/GrowableArray.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Timeline.h>

namespace floormat {

namespace {

using Search::bbox;
using Search::div_size;
using Search::div_count;
using Search::pred;

static_assert(div_count.x() == div_count.y());
static_assert((iTILE_SIZE2 % div_size).isZero());

constexpr auto chunk_bits = div_count.product(),
               visited_bits = div_count.product()*4*4;
constexpr auto div_min = -iTILE_SIZE2/2 + div_size/2;

constexpr bbox<Int> bbox_from_pos1(Vector2i center)
{
    constexpr auto half = div_size/2;
    auto start = center - half;
    return { start, start + div_size };
}

constexpr bbox<Int> bbox_from_pos2(Vector2i pt, Vector2i from)
{
    auto bb0 = bbox_from_pos1(from);
    auto bb = bbox_from_pos1(pt);
    auto min = Math::min(bb0.min, bb.min);
    auto max = Math::max(bb0.max, bb.max);
    return { min, max };
}

constexpr bbox<Int> make_pos(Vector2i ij, Vector2i from)
{
    auto pos = div_min + div_size * ij;
    auto pos0 = pos - from*div_size;
    return bbox_from_pos2(pos, pos0);
}

bool check_pos(chunk& c, const std::array<chunk*, 8>& nbs, Vector2i ij, Vector2i from, const pred& p)
{
    auto pos = make_pos(ij, from);
    bool ret = path_search::is_passable_(&c, nbs, Vector2(pos.min), Vector2(pos.max), 0, p);
    //if (ret) Debug{} << "check" << ij << pos.min << pos.max << ret;
    //Debug{} << "check" << ij << pos.min << pos.max << ret;
    return ret;
}

struct node_s
{
    Vector2i pos;
};

struct tmp_s
{
    Array<node_s> stack{NoInit, 0};
    std::bitset<visited_bits> visited;

    static uint32_t get_index(Vector2i pos);
    void append(std::bitset<chunk_bits>& passable, Vector2i pos);
    [[nodiscard]] bool check_visited(std::bitset<chunk_bits>& passable, Vector2i pos, int from);
};

uint32_t tmp_s::get_index(Vector2i pos)
{
    return (uint32_t)pos.y() * (uint32_t)div_count.x() + (uint32_t)pos.x();
}

void tmp_s::append(std::bitset<chunk_bits>& passable, Vector2i pos)
{
    auto i = get_index(pos);
    passable[i] = true;
    arrayAppend(stack, {pos});
}

bool tmp_s::check_visited(std::bitset<chunk_bits>& passable, Vector2i pos, int from)
{
    auto i = get_index(pos);
    //fm_debug_assert(i < passable.size());
    if (passable[i])
        return {};
    auto v = i*4 + (uint32_t)from;
    //fm_debug_assert(v < visited.size());
    if (visited[v])
        return {};
    visited[v] = true;
    return true;
}

tmp_s& get_tmp()
{
    static Pointer<tmp_s> tmp = [] {
        Pointer<tmp_s> p{InPlace};
        arrayReserve(p->stack, 4*div_count.product());
        return p;
    }();
    arrayResize(tmp->stack, 0);
    tmp->visited = {};
    return *tmp;
}

auto default_region_predicate(chunk& c) noexcept
{
    return [&c](collision_data data) {
        auto x = std::bit_cast<collision_data>(data);
        // XXX 'scenery' is used for all object types
        if (x.tag == (uint64_t)collision_type::scenery)
        {
            auto& w = c.world();
            auto obj = w.find_object(x.data);
            if (obj->type() == object_type::critter)
                return path_search_continue::pass;
        }
        return path_search_continue::blocked;
    };
}

} // namespace

auto chunk::make_pass_region(bool debug) -> pass_region
{
    return make_pass_region(default_region_predicate(*this), debug);
}

auto chunk::make_pass_region(const pred& f, bool debug) -> pass_region
{
    Timeline timeline;
    if (debug) [[unlikely]]
        timeline.start();

    pass_region ret;
    auto& tmp = get_tmp();
    const auto nbs = _world->neighbors(_coord);

    //if (Vector2i pos{0, 0}; check_pos(*c, nbs, pos, fours[1])) tmp.append(pos, 1); // top

    enum : uint8_t { L, R, U, D };

    const auto do_pixel = [&]<int Dir, bool Edge>(const Vector2i pos0)
    {
        constexpr Vector2i fours[4] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1}, };
        constexpr auto dir = fours[Dir];
        const auto pos = pos0 + dir;
        if constexpr(!Edge && (Dir == L || Dir == R))
            if ((uint32_t)pos.x() >= div_count.x()) [[unlikely]]
                return;
        if constexpr(!Edge && (Dir == U || Dir == D))
            if ((uint32_t)pos.y() >= div_count.y()) [[unlikely]]
                return;
        if (tmp.check_visited(ret.bits, pos, Dir) && check_pos(*this, nbs, pos, dir, f))
            tmp.append(ret.bits, pos);
    };

    for (int i = 0; i < div_count.y(); i++)
    {
        do_pixel.operator()<L, true>(Vector2i(div_count.x(), i));
        do_pixel.operator()<R, true>(Vector2i(-1, i));
        do_pixel.operator()<U, true>(Vector2i(i, div_count.y()));
        do_pixel.operator()<D, true>(Vector2i(i, -1));
    }

    while (!tmp.stack.isEmpty())
    {
        auto p = tmp.stack.back().pos;
        arrayRemoveSuffix(tmp.stack);

        do_pixel.operator()<L, false>(p);
        do_pixel.operator()<R, false>(p);
        do_pixel.operator()<U, false>(p);
        do_pixel.operator()<D, false>(p);
    }

    if (debug) [[unlikely]]
    {
        const auto time = timeline.currentFrameTime();
        DBG_nospace << "region: generating for " << _coord << " took " << fraction(1e3f*time, 3) << " ms";
    }

    return ret;
}

} // namespace floormat
