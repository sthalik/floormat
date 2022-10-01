#pragma once
#include "tile.hpp"
#include <type_traits>
#include <vector>

namespace Magnum::Examples {

constexpr inline std::size_t TILE_MAX_DIM = 16;
constexpr inline std::size_t TILE_COUNT = TILE_MAX_DIM*TILE_MAX_DIM;

struct local_coords final {
    std::uint8_t x = 0, y = 0;
    constexpr std::size_t to_index() const noexcept;
};

constexpr std::size_t local_coords::to_index() const noexcept {
    return y*TILE_MAX_DIM + x;
}

struct chunk_coords final {
    std::int16_t x = 0, y = 0;
    constexpr std::size_t to_index() const noexcept;

    static constexpr std::size_t max_bits = sizeof(chunk_coords::x)*8 * 3 / 4;
    static_assert(max_bits*4/3/8 == sizeof(decltype(chunk_coords::x)));
};

struct global_coords final {
    std::uint32_t x = 0, y = 0;
    constexpr global_coords() noexcept = default;
    constexpr global_coords(decltype(x) x, decltype(y) y) noexcept : x{x}, y{y} {}
    constexpr global_coords(chunk_coords c, local_coords tile) noexcept;
};

static_assert(std::is_same_v<decltype(chunk_coords::x), decltype(chunk_coords::y)>);
static_assert(std::is_same_v<decltype(global_coords::x), decltype(global_coords::y)>);

struct chunk final
{
    //using index_type = std::common_type_t<decltype(local_coords::x), decltype(local_coords::y)>;
    //using tile_index_array_type = std::array<index_type, TILE_COUNT>;
    //static constexpr inline local_coords center = { (index_type)(N/2), (index_type)(N/2) };

    constexpr tile& operator[](local_coords xy);
    constexpr const tile& operator[](local_coords xy) const;
    constexpr tile& operator[](std::size_t i);
    constexpr const tile& operator[](std::size_t i) const;

    // TODO use local_coords
    template<typename F>
    requires std::invocable<F, tile&, local_coords, std::size_t>
    constexpr inline void foreach_tile(F&& fun) { foreach_tile_<F, chunk&>(std::forward<F>(fun)); }

    template<typename F>
    requires std::invocable<F, const tile&, local_coords, std::size_t>
    constexpr inline void foreach_tile(F&& fun) const { foreach_tile_<F, const chunk&>(std::forward<F>(fun)); }

private:
    template<typename F, typename Self>
    constexpr void foreach_tile_(F&& fun);

    std::array<tile, TILE_COUNT> tiles = {};
};

constexpr tile& chunk::operator[](std::size_t i) {
    if (i >= TILE_COUNT)
        throw OUT_OF_RANGE(i, 0, TILE_COUNT);
    return tiles[i];
}

constexpr const tile& chunk::operator[](std::size_t i) const {
    return const_cast<chunk&>(*this).operator[](i);
}

constexpr const tile& chunk::operator[](local_coords xy) const {
    return const_cast<chunk&>(*this).operator[](xy);
}

constexpr tile& chunk::operator[](local_coords xy)
{
    auto idx = xy.to_index();
    return operator[](idx);
}

template<typename F, typename Self>
constexpr void chunk::foreach_tile_(F&& fun)
{
    constexpr auto N = TILE_MAX_DIM;
    std::size_t k = 0;
    for (std::size_t j = 0; j < N; j++)
        for (std::size_t i = 0; i < N; i++, k++)
            fun(const_cast<Self>(*this).tiles[k],
                local_coords{(std::uint8_t)i, (std::uint8_t)j},
                k);
}

constexpr std::size_t chunk_coords::to_index() const noexcept
{
    using unsigned_type = std::make_unsigned_t<decltype(x)>;
    using limits = std::numeric_limits<unsigned_type>;
    constexpr auto N = limits::max() + std::size_t{1};
    static_assert(sizeof(unsigned_type) <= sizeof(UnsignedInt)/2);
    return (std::size_t)(unsigned_type)y * N + (std::size_t)(unsigned_type)x;
}

struct hash_chunk final {
    constexpr std::size_t operator()(chunk_coords xy) const noexcept {
        return hash<sizeof(std::size_t)*8>{}(xy.to_index());
    }
};

struct world final
{
    static_assert(sizeof(chunk_coords::x) <= sizeof(std::size_t)/2);

    explicit world() = default;
    template<typename F> std::shared_ptr<chunk> ensure_chunk(chunk_coords xy, F&& fun);

private:
    std::unordered_map<chunk_coords, std::shared_ptr<chunk>, hash_chunk> chunks;
};

template<typename F>
std::shared_ptr<chunk> world::ensure_chunk(chunk_coords xy, F&& fun)
{
    ASSERT(xy.x < 1 << chunk_coords::max_bits);
    ASSERT(xy.y < 1 << chunk_coords::max_bits);

    auto it = chunks.find(xy);
    if (it != chunks.end())
        return it->second;
    else
    {
        std::shared_ptr<chunk> ptr{fun()};
        ASSERT(ptr);
        return chunks[xy] = std::move(ptr);
    }
}

constexpr global_coords::global_coords(chunk_coords c, local_coords tile) noexcept :
      x{tile.x + ((std::uint32_t)(std::make_unsigned_t<decltype(c.x)>)c.x << chunk_coords::max_bits)},
      y{tile.y + ((std::uint32_t)(std::make_unsigned_t<decltype(c.y)>)c.y << chunk_coords::max_bits)}
{
}

} // namespace Magnum::Examples
