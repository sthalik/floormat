#pragma once
#include <cstdio>
#include <concepts>
#include <Corrade/Containers/StringView.h>

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
 * 20) Just rewrite the whole thing.
 */

namespace floormat {
struct object;
struct object_proto;
} // namespace floormat

namespace floormat::Serialize {

using tilemeta = uint8_t;
using atlasid  = uint32_t;
using chunksiz = uint32_t;
using proto_t  = uint16_t;

template<typename T> struct int_traits;

template<std::unsigned_integral T> struct int_traits<T> { static constexpr T max = T(-1); };
template<std::signed_integral T> struct int_traits<T> { static constexpr T max = T(-1)&~(T(1) << sizeof(T)*8-1); };

namespace {

constexpr inline proto_t proto_version      = 20;
constexpr inline proto_t min_proto_version  = 20;
constexpr inline auto file_magic            = ".floormat.save"_s;
constexpr inline auto chunk_magic           = (uint16_t)0xdead;
constexpr inline auto object_magic          = (uint16_t)0xb00b;
constexpr inline auto atlas_magic           = (uint16_t)0xbeef;
constexpr inline auto string_max            = 256uz;
constexpr inline auto null_atlas            = (atlasid)-1;

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
    void close() noexcept { if (s) std::fclose(s); s = nullptr; }
private:
    FILE* s;
};

} // namespace

} // namespace floormat
