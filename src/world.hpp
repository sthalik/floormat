#pragma once
#include "global-coords.hpp"
#include <unordered_map>
#include <memory>

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
    static constexpr std::size_t initial_capacity = 64, collect_every = 100;
    static constexpr float max_load_factor = .5;
    std::size_t _last_collection = 0;

    void maybe_collect();
    struct hasher final { std::size_t operator()(chunk_coords c) const noexcept; };

    std::unordered_map<chunk_coords, std::shared_ptr<chunk>, hasher> _chunks{initial_capacity};
};

} // namespace floormat
