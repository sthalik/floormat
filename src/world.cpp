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

    if (auto& [c, coord2] = _last_chunk; c && coord == coord2)
    {
        return *c;
    }

    auto [it, inserted] = _chunks.try_emplace(coord);
    auto& ret = it->second;
    auto& [_c, _coord] = _last_chunk;
    _c = &ret;
    _coord = coord;
    return ret;
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
    auto& [c, _] = _last_chunk;
    c = nullptr;
}

void world::maybe_collect()
{
    if (_last_collection + collect_every > _chunks.size())
        collect();
}

void world::collect(bool force)
{
    for (auto it = _chunks.begin(); it != _chunks.end(); (void)0)
    {
        const auto& [_, c] = *it;
        if (c.empty(force))
            it = _chunks.erase(it);
        else
            ++it;
    }

    _last_collection = _chunks.size();
    auto& [c, _] = _last_chunk;
    c = nullptr;
}

} // namespace floormat
