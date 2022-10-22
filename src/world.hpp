#pragma once
#include "compat/int-hash.hpp"
#include "global-coords.hpp"
#include "tile.hpp"
#include <unordered_map>
#include <memory>
#include <optional>

namespace floormat {

struct chunk;

struct world final
{
private:
    void maybe_collect();

    static constexpr std::size_t initial_capacity = 64, collect_every = 32;
    static constexpr float max_load_factor = .5;
    static constexpr auto hasher = [](chunk_coords c) constexpr -> std::size_t {
        return int_hash((std::size_t)c.y << 16 | (std::size_t)c.x);
    };

    std::unordered_map<chunk_coords, std::shared_ptr<chunk>, decltype(hasher)> _chunks{initial_capacity, hasher};
    mutable std::optional<std::tuple<std::shared_ptr<chunk>, chunk_coords>> _last_chunk;
    std::size_t _last_collection = 0;

public:
    explicit world();

    template<typename Hash, typename Alloc, typename Pred>
    explicit world(std::unordered_map<chunk_coords, std::shared_ptr<chunk>, Hash, Alloc, Pred>&& chunks);

    std::shared_ptr<chunk> operator[](chunk_coords c) noexcept;
    std::tuple<std::shared_ptr<chunk>, tile&> operator[](global_coords pt) noexcept;
    bool contains(chunk_coords c) const noexcept;
    void clear();
    void collect();

    [[deprecated]] const auto& chunks() const noexcept {  return _chunks; } // only for serialization

    fm_DECLARE_DEPRECATED_COPY_ASSIGNMENT(world);
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(world);
};

template<typename Hash, typename Alloc, typename Pred>
world::world(std::unordered_map<chunk_coords, std::shared_ptr<chunk>, Hash, Alloc, Pred>&& chunks) :
    _chunks{chunks.begin(), chunks.end(),
            std::max(initial_capacity, std::size_t(1/max_load_factor * 2 * chunks.size())),
            hasher}
{}

} // namespace floormat
