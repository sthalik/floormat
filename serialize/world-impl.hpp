#ifndef FM_SERIALIZE_WORLD_IMPL
#error "not meant to be included directly"
#endif

#pragma once
#include "src/tile.hpp"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>

/* protocol changelog:
 *  1) Initial version.
 *  2) Tile atlas variant now always a uint8_t. Was uint16_t or uint8_t
 *     depending on value of the tile flag (1 << 6) which is now removed.
 *  3) Serialize scenery. Tile flag (1 << 6) added.
 */

namespace floormat::Serialize {

using tilemeta  = std::uint8_t;
using atlasid   = std::uint16_t;
using chunksiz  = std::uint16_t;
using proto_t = std::uint16_t;

namespace {

template<typename T> constexpr inline T int_max = std::numeric_limits<T>::max();

#define file_magic ".floormat.save"

constexpr inline std::size_t atlas_name_max = 128;
constexpr inline auto null_atlas = (atlasid)-1LL;

constexpr inline proto_t proto_version = 4;
constexpr inline proto_t min_proto_version = 1;
constexpr inline auto chunk_magic = (std::uint16_t)~0xc0d3;
constexpr inline auto scenery_magic = (std::uint16_t)~0xb00b;

using pass_mode_ = std::underlying_type_t<pass_mode>;
constexpr inline pass_mode_ pass_mask = pass_mode_COUNT - 1;
constexpr inline auto pass_bits = std::bit_width(pass_mask);

template<typename T> constexpr inline auto highbit = T(1) << sizeof(T)*8-1;
template<typename T, std::size_t N, std::size_t off>
constexpr inline auto highbits = (T(1) << N)-1 << sizeof(T)*8-N-off;

constexpr inline atlasid meta_long_scenery_bit = highbit<atlasid>;
constexpr inline atlasid meta_rotation_bits = highbits<atlasid, rotation_BITS, 1>;
constexpr inline atlasid scenery_id_flag_mask = meta_long_scenery_bit | meta_rotation_bits;
constexpr inline atlasid scenery_id_max = int_max<atlasid> & ~scenery_id_flag_mask;

} // namespace

enum : tilemeta {
    meta_ground         = 1 << 2,
    meta_wall_n         = 1 << 3,
    meta_wall_w         = 1 << 4,
    meta_short_atlasid  = 1 << 5,
    meta_short_variant_ = 1 << 6,
    meta_scenery        = 1 << 7,
};

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
