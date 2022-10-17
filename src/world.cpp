#include "world.hpp"
#include "chunk.hpp"

namespace floormat {

struct chunk_pointer_maker final
{
    operator std::shared_ptr<chunk>() const { return std::make_shared<chunk>(); }
};

world::world()
{
    _chunks.max_load_factor(max_load_factor);
}

std::shared_ptr<chunk> world::operator[](chunk_coords c) noexcept
{
    auto [it, inserted] = _chunks.try_emplace(c, chunk_pointer_maker{});
    const auto ret = it->second;
    return maybe_collect(), ret;
}

std::shared_ptr<chunk> world::maybe_chunk(chunk_coords c) noexcept
{
    if (const auto it = _chunks.find(c); it != _chunks.cend())
        return it->second;
    else
        return nullptr;
}

std::shared_ptr<const chunk> world::maybe_chunk(chunk_coords c) const noexcept
{
    return const_cast<world&>(*this).maybe_chunk(c);
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
}

} // namespace floormat
