#ifndef FM_SERIALIZE_WORLD_IMPL
#error "not meant to be included directly"
#endif

#pragma once
#include "src/tile.hpp"
#include "src/pass-mode.hpp"
#include "src/rotation.hpp"
#include "src/object-type.hpp"
#include <bit>
#include <cstdio>
#include <limits>

/* protocol changelog:
 *  1) Initial version.
 *  2) Tile atlas variant now always a uint8_t. Was uint16_t or uint8_t
 *     depending on value of the tile flag (1 << 6) which is now removed.
 *  3) Serialize scenery. Tile flag (1 << 6) added.
 *  4) Scenery dt now stored as fixed-point uint16_t.
 *  5) Serialize scenery pixel offset.
 *  6) Serialize scenery bboxes.
 *  7) Serialize scenery bbox_size offset.
 *  8) Entity subtypes.
 *  9) Interned strings.
 * 10) Chunk Z level.
 * 11) RLE empty tiles.
 * 12) Don't write object name twice.
 * 13) Entity counter initialized to 1024.
 * 14) Always store object offset, rework how sc_exact works.
 * 15) Add light alpha.
 * 16) One more bit for light falloff enum.
 * 17) Switch critter::offset_frac to unsigned.
 */

namespace floormat {
struct object;
struct object_proto;
} // namespace floormat

namespace floormat::Serialize {

using tilemeta = uint8_t;
using atlasid  = uint16_t;
using chunksiz = uint16_t;
using proto_t  = uint16_t;

namespace {

template<typename T> constexpr inline T int_max = std::numeric_limits<T>::max();

#define file_magic ".floormat.save"

constexpr inline proto_t proto_version = 17;

constexpr inline size_t atlas_name_max = 128;
constexpr inline auto null_atlas = (atlasid)-1LL;

constexpr inline size_t critter_name_max = 128;
constexpr inline size_t string_max = 512;

constexpr inline proto_t min_proto_version = 1;
constexpr inline auto chunk_magic = (uint16_t)~0xc0d3;
constexpr inline auto scenery_magic = (uint16_t)~0xb00b;

using pass_mode_i = std::underlying_type_t<pass_mode>;
constexpr inline pass_mode_i pass_mask = (1 << pass_mode_BITS)-1;
using object_type_i = std::underlying_type_t<object_type>;

template<typename T, size_t N, size_t off>
constexpr inline auto highbits = (T(1) << N)-1 << sizeof(T)*8-N-off;

template<size_t N, typename T = uint8_t>
constexpr T lowbits = T((T{1} << N)-T{1});

constexpr inline atlasid meta_short_scenery_bit = highbits<atlasid, 1, 0>;
constexpr inline atlasid meta_rotation_bits = highbits<atlasid, rotation_BITS, 1>;
constexpr inline atlasid scenery_id_flag_mask = meta_short_scenery_bit | meta_rotation_bits;
constexpr inline atlasid scenery_id_max = int_max<atlasid> & ~scenery_id_flag_mask;

} // namespace

template<typename T> concept object_subtype = std::is_base_of_v<object, T> || std::is_base_of_v<object_proto, T>;

enum : tilemeta {
    meta_ground         = 1 << 2,
    meta_wall_n         = 1 << 3,
    meta_wall_w         = 1 << 4,
    meta_rle            = 1 << 7,

    meta_short_atlasid_ = 1 << 5,
    meta_short_variant_ = 1 << 6,
    meta_scenery_       = 1 << 7,
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
