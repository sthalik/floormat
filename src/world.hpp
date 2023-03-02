#pragma once
#include "compat/int-hash.hpp"
#include "compat/defs.hpp"
#include "chunk.hpp"
#include "global-coords.hpp"
#include <cstddef>
#include <unordered_map>
#include <memory>

namespace floormat {

struct world final
{
private:
    struct chunk_tuple final {
        static constexpr chunk_coords invalid_coords = { -1 << 15, -1 << 15 };
        chunk* c = nullptr;
        chunk_coords pos = invalid_coords;
    } _last_chunk;

    static constexpr std::size_t initial_capacity = 64;
    static constexpr float max_load_factor = .5;
    static constexpr auto hasher = [](chunk_coords c) constexpr -> std::size_t {
        return int_hash((std::size_t)c.y << 16 | (std::size_t)c.x);
    };
    std::unordered_map<chunk_coords, chunk, decltype(hasher)> _chunks;
    std::size_t _last_collection = 0;
    std::size_t _collect_every = 64;

    explicit world(std::size_t capacity);

public:
    explicit world();

    struct pair final { chunk& c; tile_ref t; }; // NOLINT

    template<typename Hash, typename Alloc, typename Pred>
    explicit world(std::unordered_map<chunk_coords, chunk, Hash, Alloc, Pred>&& chunks);

    chunk& operator[](chunk_coords c) noexcept;
    pair operator[](global_coords pt) noexcept;
    bool contains(chunk_coords c) const noexcept;
    void clear();
    void collect(bool force = false);
    void maybe_collect();
    std::size_t size() const noexcept { return _chunks.size(); }

    [[deprecated]] const auto& chunks() const noexcept { return _chunks; } // only for serialization

    void serialize(StringView filename);
    static world deserialize(StringView filename);
    void set_collect_threshold(std::size_t value) { _collect_every = value; }
    std::size_t collect_threshold() const noexcept { return _collect_every; }

    fm_DECLARE_DEPRECATED_COPY_ASSIGNMENT(world);
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(world);
};

template<typename Hash, typename Alloc, typename Pred>
world::world(std::unordered_map<chunk_coords, chunk, Hash, Alloc, Pred>&& chunks) :
    world{std::max(initial_capacity, std::size_t(1/max_load_factor * 2 * chunks.size()))}
{
    for (auto&& [coord, c] : chunks)
        operator[](coord) = std::move(c);
}

} // namespace floormat
