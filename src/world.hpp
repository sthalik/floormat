#pragma once
#include "compat/int-hash.hpp"
#include "global-coords.hpp"
#include <unordered_map>
#include <memory>

namespace std {

template<typename> struct hash;

template<>
struct hash<floormat::chunk_coords> final
{
    constexpr
    std::size_t operator()(floormat::chunk_coords c) const noexcept
    {
        return floormat::int_hash((std::size_t)c.y << 16 | (std::size_t)c.x);
    }
};

} // namespace std

namespace floormat {

struct chunk;

struct world final
{
    world();
    std::shared_ptr<chunk> operator[](chunk_coords c) noexcept;
    std::shared_ptr<chunk> maybe_chunk(chunk_coords c) noexcept;
    std::shared_ptr<const chunk> maybe_chunk(chunk_coords c) const noexcept;
    bool contains(chunk_coords c) const noexcept;
    void clear();
    void collect();

private:
    static constexpr std::size_t initial_capacity = 64, collect_every = 32;
    static constexpr float max_load_factor = .5;
    std::size_t _last_collection = 0;

    void maybe_collect();

    std::unordered_map<chunk_coords, std::shared_ptr<chunk>> _chunks{initial_capacity};
};

} // namespace floormat
