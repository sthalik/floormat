#include "chunk-region.hpp"
#include "path-search-bbox.hpp"
#include "world.hpp"
#include <Corrade/Containers/GrowableArray.h>
#include <Magnum/Math/Functions.h>

namespace floormat {

namespace {

using detail_astar::bbox;
using detail_astar::div_factor;
using detail_astar::div_size;
using detail_astar::div_count;

static_assert(div_count.x() == div_count.y());
static_assert((iTILE_SIZE2 % div_size).isZero());

constexpr auto chunk_bits = div_count.product(),
               visited_bits = div_count.product()*4*4;
constexpr auto div_min = -iTILE_SIZE2/2 + div_size/2,
               div_max = iTILE_SIZE2 * TILE_MAX_DIM - iTILE_SIZE2/2 - div_size + div_size/2;

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
    auto pos0 = pos + from*div_size;
    return bbox_from_pos2(pos, pos0);
}

bool check_pos(chunk& c, const std::array<chunk*, 8>& nbs, Vector2i ij, Vector2i from)
{
    auto pos = make_pos(ij, from);
    bool ret = path_search::is_passable_(&c, nbs, Vector2(pos.min), Vector2(pos.max), 0);
    //if (ret) Debug{} << "check" << ij << ij/div_factor << ij % div_factor << pos.min << pos.max << ret;
    //Debug{} << "check" << ij << pos.min << pos.max << ret;
    return ret;
}

struct node_s
{
    Vector2i pos;
};

struct tmp_s
{
    Array<node_s> stack;
    std::bitset<visited_bits> visited;

    static uint32_t get_index(Vector2i pos);
    void append(std::bitset<chunk_bits>& passable, Vector2i pos);
    [[nodiscard]] bool check_visited(std::bitset<chunk_bits>& passable, Vector2i pos, int from);
    void clear();
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

void tmp_s::clear()
{
    arrayResize(stack, 0);
    visited = {};
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

} // namespace

void chunk::delete_pass_region(pass_region*& ptr)
{
    if (ptr)
    {
        delete ptr;
        ptr = nullptr;
    }
}

auto chunk::get_pass_region() -> const pass_region*
{
    if (!_region_modified)
    {
        fm_debug_assert(_region != nullptr);
        return _region;
    }

    if (!_region)
        _region = new pass_region;
    else
        _region->bits = {};

    make_pass_region(*_region);
    return _region;
}

bool chunk::is_region_modified() const noexcept { return _region_modified; }
void chunk::mark_region_modified() noexcept { _region_modified = true; }

void chunk::make_pass_region(pass_region& ret)
{
    auto& tmp = get_tmp();
    const auto nbs = _world->neighbors(_coord);

    constexpr Vector2i fours[4] = { {0, 1}, {0, -1}, {1, 0}, {-1, 0} };
    constexpr auto last = div_count - Vector2i{1};
    //if (Vector2i pos{0, 0}; check_pos(*c, nbs, pos, fours[1])) tmp.append(pos, 1); // top

    for (int i = 0; i < div_count.x(); i++)
    {
        if (Vector2i pos{i, last.y()}; check_pos(*this, nbs, pos, fours[0])) tmp.append(ret.bits, pos); // bottom
        if (Vector2i pos{i, 0};        check_pos(*this, nbs, pos, fours[1])) tmp.append(ret.bits, pos); // top
        if (Vector2i pos{last.x(), i}; check_pos(*this, nbs, pos, fours[2])) tmp.append(ret.bits, pos); // right
        if (Vector2i pos{0, i};        check_pos(*this, nbs, pos, fours[3])) tmp.append(ret.bits, pos); // left
    }

    while (!tmp.stack.isEmpty())
    {
        auto p = tmp.stack.back().pos;
        arrayRemoveSuffix(tmp.stack);
        for (int i = 0; i < 4; i++)
        {
            Vector2i from = fours[i], pos{p - from};
            if ((uint32_t)pos.x() >= div_count.x() || (uint32_t)pos.y() >= div_count.y()) [[unlikely]]
                continue;
            if (tmp.check_visited(ret.bits, pos, i) && check_pos(*this, nbs, pos, from))
                tmp.append(ret.bits, pos);
        }
    }
}

} // namespace floormat
