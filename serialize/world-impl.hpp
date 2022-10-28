#ifndef FM_SERIALIZE_WORLD_IMPL
#error "not meant to be included directly"
#endif

#pragma once
#include "src/tile.hpp"

namespace floormat::Serialize {

namespace {

using tilemeta  = std::uint8_t;
using imgvar    = decltype(tile_image::variant);
using atlasid   = std::uint16_t;
using chunksiz  = std::uint16_t;
using enum tile::pass_mode;

template<typename T> constexpr inline T int_max = std::numeric_limits<T>::max();

#define file_magic ".floormat.save"

constexpr inline std::size_t atlas_name_max = 128;
constexpr inline auto null_atlas = (atlasid)-1LL;

constexpr inline std::uint16_t proto_version = 1;
constexpr inline auto chunk_magic = (std::uint16_t)~0xc0d3;

constexpr inline std::underlying_type_t<tile::pass_mode> pass_mask = pass_blocked | pass_shoot_through | pass_ok;
constexpr inline auto pass_bits = std::bit_width(pass_mask);

enum : tilemeta {
    meta_ground     = 1 << (pass_bits + 0),
    meta_wall_n     = 1 << (pass_bits + 1),
    meta_wall_w     = 1 << (pass_bits + 2),
};

} // namespace

} // namespace floormat::Serialize

namespace floormat {

namespace {

struct FILE_raii final {
    FILE_raii(FILE* s) noexcept : s{s} {}
    ~FILE_raii() noexcept { close(); }
    operator FILE*() noexcept { return s; }
    void close() noexcept { if (s) ::fclose(s); s = nullptr; }
private:
    FILE* s;
};

} // namespace

} // namespace floormat
