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
    maybe_collect();    return it->second;
}

std::shared_ptr<const chunk> world::maybe_chunk(chunk_coords c) const noexcept
{
    if (const auto it = _chunks.find(c); it != _chunks.cend())
        return it->second;
    else
        return nullptr;
}

std::shared_ptr<chunk> world::maybe_chunk(chunk_coords c) noexcept
{
    return std::const_pointer_cast<chunk>(const_cast<const world&>(*this).maybe_chunk(c));
}

bool world::contains(chunk_coords c) const noexcept
{
    return _chunks.find(c) != _chunks.cend();
}

void world::clear()
{
    _last_collection = 0;
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
        const auto& [k, c] = *it;
        if (c->empty())
            it = _chunks.erase(it);
        else
            it++;
    }
    _last_collection = _chunks.size();
}

std::size_t world::hasher::operator()(chunk_coords c) const noexcept
{
    std::size_t x = (std::size_t)c.y << 16 | (std::size_t)c.x;

    if constexpr(sizeof(std::size_t) == 4)
    {
        // by Chris Wellons <https://nullprogram.com/blog/2018/07/31/>
        x ^= x >> 15;
        x *= 0x2c1b3c6dU;
        x ^= x >> 12;
        x *= 0x297a2d39U;
        x ^= x >> 15;
    }
    else if constexpr(sizeof(std::size_t) == 8)
    {
        // splitmix64 by George Marsaglia
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9U;
        x ^= x >> 27;
        x *= 0x94d049bb133111ebU;
        x ^= x >> 31;
    }

    return x;
}

} // namespace floormat
