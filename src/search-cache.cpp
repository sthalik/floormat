#include "search-cache.hpp"
#include "search-constants.hpp"
#include "world.hpp"
#include <array>
#include <bitset>

namespace floormat::Search {

struct cache::chunk_cache
{
    static constexpr size_t dimensions[] = {
        TILE_COUNT,
        (size_t)div_factor * (size_t)div_factor,
    };
    static constexpr size_t size = []() constexpr -> size_t {
        size_t x = 1;
        for (auto i : dimensions)
            x *= i;
        return x;
    }();
    static constexpr size_t rank = arraySize(dimensions);

    struct index { uint32_t value = 0; };
    class chunk* chunk = nullptr;
    std::array<index, size> indexes = {};
    std::bitset<size> exists{false};
};

cache::cache() = default;

Vector2ui cache::get_size_to_allocate(uint32_t max_dist)
{
    constexpr auto chunk_size = Vector2ui(iTILE_SIZE2) * TILE_MAX_DIM;
    constexpr auto rounding   = chunk_size - Vector2ui(1);
    auto nchunks = (Vector2ui(max_dist) + rounding) / chunk_size;
    return nchunks + Vector2ui(3);
}

void cache::allocate(point from, uint32_t max_dist)
{
    auto off = get_size_to_allocate(max_dist);
    start = Vector2i(from.chunk()) - Vector2i(off);
    size = off * 2u + Vector2ui(1);
    auto len = size.product();
    if (len > array.size())
        array = Array<chunk_cache>{ValueInit, len};
    else
        for (auto i = 0uz; i < len; i++)
        {
            array[i].chunk = {};
            array[i].exists = {};
        }
}

size_t cache::get_chunk_index(Vector2i start, Vector2ui size, Vector2i coord)
{
    auto off = Vector2ui(coord - start);
    fm_assert(off < size);
    auto index = off.y() * size.x() + off.x();
    fm_debug_assert(index < size.product());
    return index;
}

size_t cache::get_chunk_index(Vector2i chunk) const { return get_chunk_index(start, size, chunk); }

size_t cache::get_tile_index(Vector2i pos, Vector2b offset_)
{
    Vector2i offset{offset_};
    constexpr auto tile_start = div_size * div_factor/-2;
    offset -= tile_start;
    fm_debug_assert(offset >= Vector2i{0, 0} && offset < div_size * div_factor);
    auto nth_div = Vector2ui(offset) / Vector2ui(div_size);
    const size_t idx[] = {
        (size_t)pos.y() * TILE_MAX_DIM + (size_t)pos.x(),
        (size_t)nth_div.y() * div_factor + (size_t)nth_div.x(),
    };
    size_t index = 0;
    for (auto i = 0uz; i < chunk_cache::rank; i++)
    {
        size_t k = idx[i];
        for (auto j = 0uz; j < i; j++)
            k *= chunk_cache::dimensions[j];
        index += k;
    }
    fm_debug_assert(index < chunk_cache::size);
    return index;
}

void cache::add_index(size_t chunk_index, size_t tile_index, uint32_t index)
{
    fm_debug_assert(index != (uint32_t)-1);
    auto& c = array[chunk_index];
    fm_debug_assert(!c.exists[tile_index]);
    c.exists[tile_index] = true;
    c.indexes[tile_index] = {index};
}

void cache::add_index(point pt, uint32_t index)
{
    auto ch = get_chunk_index(Vector2i(pt.chunk()));
    auto tile = get_tile_index(Vector2i(pt.local()), pt.offset());
    fm_debug_assert(!array[ch].exists[tile]);
    array[ch].exists[tile] = true;
    array[ch].indexes[tile] = {index};
}

uint32_t cache::lookup_index(size_t chunk_index, size_t tile_index)
{
    auto& c = array[chunk_index];
    if (c.exists[tile_index])
        return c.indexes[tile_index].value;
    else
        return (uint32_t)-1;
}

chunk* cache::try_get_chunk(world& w, floormat::chunk_coords_ ch)
{
    auto idx = get_chunk_index({ch.x, ch.y});
    auto& page = array[idx];
    if (page.chunk == (chunk*)-1)
        return nullptr;
    else if (!page.chunk)
    {
        page.chunk = w.at(ch);
        if (!page.chunk)
        {
            page.chunk = (chunk*)-1;
            return nullptr;
        }
        return page.chunk;
    }
    else
        return page.chunk;
}

std::array<chunk*, 8> cache::get_neighbors(world& w, chunk_coords_ ch0)
{
    fm_debug_assert(!size.isZero());
    std::array<chunk*, 8> neighbors;
    for (auto i = 0u; i < 8; i++)
        neighbors[i] = try_get_chunk(w, ch0 + world::neighbor_offsets[i]);
    return neighbors;
}

} // namespace floormat::Search
