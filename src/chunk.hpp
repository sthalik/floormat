#pragma once
#include "tile.hpp"
#include "tile-iterator.hpp"
#include "scenery.hpp"
#include <type_traits>
#include <array>
#include <vector>
#include <memory>
#include <Magnum/GL/Mesh.h>

namespace loose_quadtree {
template<typename Number, typename Object, typename BBExtractor> class LooseQuadtree;
template<typename Number, typename Object, typename BBExtractor> struct Query;
template<typename Number> struct BoundingBox;
template<typename Number> struct TrivialBBExtractor;
} // namespace loose_quadtree

namespace floormat {

struct anim_atlas;
template<typename Num, typename BB, typename BBE> struct collision_iterator;
template<typename Num, typename BB, typename BBE> struct collision_query;
struct collision_bbox;
struct compact_bb;
struct compact_bb_extractor;

enum class collision : std::uint8_t {
    view, shoot, move,
};

struct chunk final
{
    friend struct tile_ref;

    tile_ref operator[](std::size_t idx) noexcept;
    tile_proto operator[](std::size_t idx) const noexcept;
    tile_ref operator[](local_coords xy) noexcept;
    tile_proto operator[](local_coords xy) const noexcept;

    using iterator = tile_iterator;
    using const_iterator = tile_const_iterator;

    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;

    bool empty(bool force = false) const noexcept;

    chunk() noexcept;
    ~chunk() noexcept;
    chunk(const chunk&) = delete;
    chunk& operator=(const chunk&) = delete;
    chunk(chunk&&) noexcept;
    chunk& operator=(chunk&&) noexcept;

    void mark_ground_modified() noexcept;
    void mark_walls_modified() noexcept;
    void mark_scenery_modified() noexcept;
    void mark_modified() noexcept;

    struct ground_mesh_tuple final {
        GL::Mesh& mesh;                                     // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
        const std::array<std::uint8_t, TILE_COUNT>& ids;    // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    };
    struct wall_mesh_tuple final {
        GL::Mesh& mesh;                                     // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
        const std::array<std::uint16_t, TILE_COUNT*2>& ids; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    };

    ground_mesh_tuple ensure_ground_mesh() noexcept;
    tile_atlas* ground_atlas_at(std::size_t i) const noexcept;

    wall_mesh_tuple ensure_wall_mesh() noexcept;
    tile_atlas* wall_atlas_at(std::size_t i) const noexcept;

    void ensure_passability() noexcept;

    static constexpr inline bool lqt_compact_bb = sizeof(void*) >= 8;
    //static constexpr inline bool lqt_compact_bb = false;
    using BB  = std::conditional_t<lqt_compact_bb, compact_bb, loose_quadtree::BoundingBox<std::int16_t>>;
    using BBE = std::conditional_t<lqt_compact_bb, compact_bb_extractor, loose_quadtree::TrivialBBExtractor<std::int16_t>>;
    using lqt = loose_quadtree::LooseQuadtree<std::int16_t, BB, BBE>;
    using Query = collision_query<std::int16_t, BB, BBE>;

    Query query_collisions(Vector2s position, Vector2us size, collision type) const;
    Query query_collisions(local_coords p, Vector2us size, Vector2s offset, collision type) const;
    Query query_collisions(Vector4s vec, collision type) const;

private:
    std::array<std::shared_ptr<tile_atlas>, TILE_COUNT> _ground_atlases;
    std::array<std::uint8_t, TILE_COUNT> ground_indexes = {};
    std::array<variant_t, TILE_COUNT> _ground_variants = {};
    std::array<std::shared_ptr<tile_atlas>, TILE_COUNT*2> _wall_atlases;
    std::array<std::uint16_t, TILE_COUNT*2> wall_indexes = {};
    std::array<variant_t, TILE_COUNT*2> _wall_variants = {};
    std::array<std::shared_ptr<anim_atlas>, TILE_COUNT> _scenery_atlases;
    std::array<scenery, TILE_COUNT> _scenery_variants = {};

    template<bool> struct lqt_ops;

    std::unique_ptr<lqt> _lqt_move, _lqt_shoot, _lqt_view;
    std::conditional_t<lqt_compact_bb, unsigned char, std::vector<loose_quadtree::BoundingBox<std::int16_t>>> _bboxes;

    GL::Mesh ground_mesh{NoCreate}, wall_mesh{NoCreate};
    mutable bool _maybe_empty      : 1 = true,
                 _ground_modified  : 1 = true,
                 _walls_modified   : 1 = true,
                 _scenery_modified : 1 = true,
                 _pass_modified    : 1 = true;

    lqt& lqt_from_collision_type(collision type) const noexcept;
    static std::unique_ptr<lqt> make_lqt();
    void cleanup_lqt();
};

} // namespace floormat
