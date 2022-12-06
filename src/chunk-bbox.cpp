#include "chunk.hpp"
#include "compat/LooseQuadtree-impl.h"
#include "src/tile-atlas.hpp"
#include "src/collision.hpp"
#include <bit>
#include <Magnum/Math/Vector4.h>

namespace floormat {

template<>
struct chunk::lqt_ops<false>
{
    using BB = loose_quadtree::BoundingBox<std::int16_t>;
    using TBBE = loose_quadtree::TrivialBBExtractor<std::int16_t>;
    static BB* insert(const BB& bbox, std::vector<BB>& vec)
    {
        vec.push_back(bbox);
        return &vec.back();
    }
    static void clear_vec(std::vector<BB>& vec)
    {
        vec.clear();
    }
    static void reserve_vec(std::vector<BB>& vec, std::size_t size)
    {
        vec.reserve(size);
    }
};

template<>
struct chunk::lqt_ops<true>
{
    using BB = loose_quadtree::BoundingBox<std::int16_t>;
    static compact_bb* insert(const BB& bbox, unsigned char&)
    {
        if constexpr(sizeof(void*) >= sizeof(BB))
            return std::bit_cast<compact_bb*>(bbox);
        else
            return {};
    }
    static void clear_vec(unsigned char&) {}
    static void reserve_vec(unsigned char&, std::size_t) {}
};

struct collision_bbox final
{
    using BB = loose_quadtree::BoundingBox<std::int16_t>;
    operator BB() const noexcept;

    std::int16_t left = 0, top = 0;
    std::uint16_t width = 0, height = 0;
    enum pass_mode pass_mode = pass_mode::pass;
};

collision_bbox::operator BB() const noexcept
{
    return { left, top, (std::int16_t)width, (std::int16_t)height };
}

static constexpr Vector2s tile_start(std::size_t k)
{
    const auto i = std::uint8_t(k);
    const local_coords coord{i};
    return sTILE_SIZE2 * Vector2s(coord.x, coord.y) - sTILE_SIZE2/2;
}

void chunk::ensure_passability() noexcept
{
    if (!_pass_modified)
        return;
    _pass_modified = false;

    if (!_lqt_move)
        _lqt_move = make_lqt();
    if (!_lqt_shoot)
        _lqt_shoot = make_lqt();
    if (!_lqt_view)
        _lqt_view = make_lqt();

    using ops = lqt_ops<lqt_compact_bb>;

    _lqt_move->Clear();
    _lqt_shoot->Clear();
    _lqt_view->Clear();
    ops::clear_vec(_bboxes);

    std::vector<collision_bbox> bboxes;
    if constexpr(!lqt_compact_bb)
        bboxes.reserve(TILE_COUNT*4);

    constexpr auto whole_tile = [](std::size_t k, pass_mode p) constexpr -> collision_bbox {
        auto start = tile_start(k);
        return { start[0], start[1], usTILE_SIZE2[0], usTILE_SIZE2[1], p };
    };

    constexpr auto wall_north = [](std::size_t k, pass_mode p) constexpr -> collision_bbox {
        auto start = tile_start(k) - Vector2s(0, 1);
        return { start[0], start[1], usTILE_SIZE2[0], 2, p };
    };

    constexpr auto wall_west = [](std::size_t k, pass_mode p) constexpr -> collision_bbox {
        auto start = tile_start(k) - Vector2s(1, 0);
        return { start[0], start[1], 2, usTILE_SIZE2[1], p };
    };

    for (std::size_t i = 0; i < TILE_COUNT; i++)
    {
        const auto tile = const_cast<chunk&>(*this)[i];
        if (auto s = tile.scenery())
            if (auto p = s.frame.passability; p != pass_mode::pass)
                bboxes.push_back(whole_tile(i, p));
        if (auto atlas = tile.ground_atlas())
            if (auto p = atlas->pass_mode(pass_mode::pass); p != pass_mode::pass)
                bboxes.push_back(whole_tile(i, p));
        if (auto atlas = tile.wall_north_atlas())
            if (auto p = atlas->pass_mode(pass_mode::blocked); p != pass_mode::pass)
                bboxes.push_back(wall_north(i, p));
        if (auto atlas = tile.wall_west_atlas())
            if (auto p = atlas->pass_mode(pass_mode::blocked); p != pass_mode::pass)
                bboxes.push_back(wall_west(i, p));
    }

    ops::reserve_vec(_bboxes, bboxes.size());

    for (const collision_bbox& bbox : bboxes)
    {
        auto* ptr = ops::insert(bbox, _bboxes);

        switch (bbox.pass_mode)
        {
        case pass_mode::blocked:
            _lqt_view->Insert(ptr);
            [[fallthrough]];
        case pass_mode::see_through:
            _lqt_shoot->Insert(ptr);
            [[fallthrough]];
        case pass_mode::shoot_through:
            _lqt_move->Insert(ptr);
            break;
        case pass_mode::pass:
            break;
        }
    }
}

auto chunk::query_collisions(Vector4s vec, collision type) const -> Query
{
    const_cast<chunk&>(*this).ensure_passability();
    loose_quadtree::BoundingBox<std::int16_t> bbox { vec[0], vec[1], vec[2], vec[3] };
    return { lqt_from_collision_type(type).QueryIntersectsRegion(bbox) };
}

auto chunk::query_collisions(Vector2s position, Vector2us size, collision type) const -> Query
{
    const_cast<chunk&>(*this).ensure_passability();
    constexpr auto half = sTILE_SIZE2/2;
    const auto start = position - half;
    loose_quadtree::BoundingBox<std::int16_t> bbox {start[0], start[1], (Short)size[0], (Short)size[1] };
    return { lqt_from_collision_type(type).QueryIntersectsRegion(bbox) };
}

auto chunk::query_collisions(local_coords p, Vector2us size, Vector2s offset, collision type) const -> Query
{
    const_cast<chunk&>(*this).ensure_passability();
    const auto pos = Vector2s(p.x, p.y) * sTILE_SIZE2 + offset;
    const auto start = pos - Vector2s(size/2);
    loose_quadtree::BoundingBox<std::int16_t> bbox { start[0], start[1], (Short)size[0], (Short)size[1] };
    return { lqt_from_collision_type(type).QueryIntersectsRegion(bbox) };
}

auto chunk::lqt_from_collision_type(collision type) const noexcept -> lqt&
{
    switch (type)
    {
    case collision::move:
        return *_lqt_move;
    case collision::shoot:
        return *_lqt_shoot;
    case collision::view:
        return *_lqt_view;
    }
    fm_abort("wrong collision type '%hhu'", std::uint8_t(type));
}

auto chunk::make_lqt() -> std::unique_ptr<lqt>
{
    return std::make_unique<lqt>();
}

void chunk::cleanup_lqt()
{
    if (_lqt_move)
        _lqt_move->ForceCleanup();
    if (_lqt_shoot)
        _lqt_shoot->ForceCleanup();
    if (_lqt_view)
        _lqt_view->ForceCleanup();
}

} // namespace floormat
