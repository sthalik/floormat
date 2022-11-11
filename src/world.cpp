#include "world.hpp"
#include "chunk.hpp"

namespace floormat {

world::world() : world{initial_capacity}
{
}

world::world(std::size_t capacity) : _chunks{capacity, hasher}
{
    _chunks.max_load_factor(max_load_factor);
}

fm_noinline
chunk& world::operator[](chunk_coords coord) noexcept
{
    maybe_collect();
    auto& [c, coord2] = _last_chunk;
    if (coord != coord2)
        c = &_chunks.try_emplace(coord).first->second;
    coord2 = coord;
    return *c;
}

auto world::operator[](global_coords pt) noexcept -> pair
{
    auto& c = operator[](pt.chunk());
    return { c, c[pt.local()] };
}

bool world::contains(chunk_coords c) const noexcept
{
    return _chunks.find(c) != _chunks.cend();
}

void world::clear()
{
    _last_collection = 0;
    _chunks.clear();
    _chunks.rehash(initial_capacity);
    auto& [c, pos] = _last_chunk;
    c = nullptr;
    pos = chunk_tuple::invalid_coords;
}

void world::maybe_collect()
{
    if (_chunks.size() > _last_collection + collect_every)
        collect();
}

void world::collect(bool force)
{
    const auto len0 = _chunks.size();
    for (auto it = _chunks.begin(); it != _chunks.end(); (void)0)
    {
        const auto& [_, c] = *it;
        if (c.empty(force))
            it = _chunks.erase(it);
        else
            ++it;
    }

    _last_collection = _chunks.size();
    auto& [c, pos] = _last_chunk;
    c = nullptr;
    pos = chunk_tuple::invalid_coords;
    const auto len = len0 - _chunks.size();
    if (len)
        fm_debug("world: collected %zu/%zu chunks", len, len0);
}

} // namespace floormat
