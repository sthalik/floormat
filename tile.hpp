#pragma once
#include "atlas.hpp"
#include "hash.hpp"
#include "defs.hpp"

#include <cstddef>
#include <tuple>
#include <array>
#include <memory>
#include <unordered_map>
#include <utility>

namespace Magnum::Examples {

struct tile_image final
{
    std::shared_ptr<atlas_texture> atlas;
    int variant = -1;
};

struct tile final
{
    enum class pass_mode : std::uint8_t { pass_blocked, pass_yes, pass_shoot_through, pass_obscured };
    using enum pass_mode;

    tile_image ground_image_;
    pass_mode passability_ = pass_obscured;

    explicit operator bool() const noexcept { return !!ground_image_.atlas; }
};

struct local_coords final {
    std::uint8_t x = 0, y = 0;
    constexpr std::size_t to_index() const noexcept;
};

struct chunk_coords final {
    Short x = 0, y = 0;
    constexpr std::size_t to_index() const noexcept;
};

struct chunk final
{
    static constexpr std::size_t N = 16;
    static constexpr std::size_t TILE_COUNT = N*N;

    using index_type = decltype(local_coords{}.x);
    using tile_index_array_type = std::array<index_type, TILE_COUNT>;
    //static constexpr inline local_coords center = { (index_type)(N/2), (index_type)(N/2) };

    std::array<index_type, TILE_COUNT> indices = make_tile_indices();

    constexpr struct tile& tile(local_coords xy);
    constexpr struct tile& tile(local_coords xy) const { return const_cast<chunk&>(*this).tile(xy); }
    constexpr struct tile& operator[](std::size_t i);

    template<typename F> constexpr inline void foreach_tile(F&& fun) { foreach_tile_<F, chunk&>(fun); }
    template<typename F> constexpr inline void foreach_tile(F&& fun) const { foreach_tile_<F, const chunk&>(fun); }

private:
    template<typename F, typename Self> constexpr void foreach_tile_(F&& fun);
    static std::array<index_type, TILE_COUNT> make_tile_indices() noexcept;

    std::array<struct tile, TILE_COUNT> tiles = {};
};

constexpr std::size_t local_coords::to_index() const noexcept {
    return y*chunk::N + x;
}

constexpr struct tile& chunk::operator[](std::size_t i) {
    if (i >= TILE_COUNT)
        throw OUT_OF_RANGE(i, 0, TILE_COUNT);
    return tiles[i];
}

constexpr tile& chunk::tile(local_coords xy)
{
    auto idx = xy.to_index();
    if (idx >= TILE_COUNT)
        throw OUT_OF_RANGE(idx, 0, TILE_COUNT);
    return tiles[idx];
}

template<typename F, typename Self>
constexpr void chunk::foreach_tile_(F&& fun)
{
    for (unsigned j = 0; j < N; j++)
        for (unsigned i = 0; i < N; i++)
        {
            unsigned idx = j*N + i;
            if (tiles[idx])
                fun(*static_cast<Self>(*this).tiles[idx]);
        }
}

constexpr std::size_t chunk_coords::to_index() const noexcept
{
    using unsigned_type = std::make_unsigned_t<decltype(x)>;
    using limits = std::numeric_limits<unsigned_type>;
    constexpr auto N = limits::max() + std::size_t{1};
    static_assert(sizeof(unsigned_type) <= sizeof(std::size_t)/2);
    return (std::size_t)(unsigned_type)y * N + (std::size_t)(unsigned_type)x;
}

struct hash_chunk final {
    constexpr std::size_t operator()(chunk_coords xy) const noexcept {
        return hash<sizeof(std::size_t)*8>{}(xy.to_index());
    }
};

struct world final
{
    explicit world();
    template<typename F> std::shared_ptr<chunk> ensure_chunk(chunk_coords xy, F&& fun);

private:
    std::unordered_map<chunk_coords, std::shared_ptr<chunk>, hash_chunk> chunks;
};

template<typename F>
std::shared_ptr<chunk> world::ensure_chunk(chunk_coords xy, F&& fun)
{
    auto it = chunks.find(xy);
    if (it != chunks.end())
        return it->second;
    else
        return chunks[xy] = fun();
}

} //namespace Magnum::Examples
