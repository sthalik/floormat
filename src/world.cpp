#include "world.hpp"
#include "chunk.hpp"

namespace floormat {

struct chunk_pointer_maker final
{
    inline operator std::shared_ptr<chunk>() const { return std::make_shared<chunk>(); }
};

world::world()
{
    _chunks.max_load_factor(max_load_factor);
}

std::shared_ptr<chunk> world::operator[](chunk_coords c) noexcept
{
    maybe_collect();

    if (_last_chunk)
    {
        auto& [ret, pos] = *_last_chunk;
        if (pos == c)
            return ret;
    }

    auto [it, inserted] = _chunks.try_emplace(c, chunk_pointer_maker{});
    auto ret = it->second;
    _last_chunk = { ret, c };
    return ret;
}

std::tuple<std::shared_ptr<chunk>, tile&> world::operator[](global_coords pt) noexcept
{
    auto c = operator[](pt.chunk());
    return { c, (*c)[pt.local()] };
}

bool world::contains(chunk_coords c) const noexcept
{
    return _chunks.find(c) != _chunks.cend();
}

void world::clear()
{
    _last_collection = 0;
    _last_chunk = std::nullopt;
    _chunks.clear();
    _chunks.rehash(initial_capacity);
}

void world::maybe_collect()
{
    if (_last_collection + collect_every > _chunks.size())
        collect();
}

void world::collect()
{
    for (auto it = _chunks.begin(); it != _chunks.end(); (void)0)
    {
        const auto& [_, c] = *it;
        if (c->empty())
            it = _chunks.erase(it);
        else
            ++it;
    }

    _last_collection = _chunks.size();
    _last_chunk = std::nullopt;
}

} // namespace floormat
