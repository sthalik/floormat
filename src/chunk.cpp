#include "chunk.hpp"
#include "compat/LooseQuadtree-impl.h"
#include "src/tile-atlas.hpp"

namespace floormat {

bool chunk::empty(bool force) const noexcept
{
    if (!force && !_maybe_empty)
        return false;

    for (std::size_t i = 0; i < TILE_COUNT; i++)
    {
        if (_ground_atlases[i] || _wall_atlases[i*2 + 0] || _wall_atlases[i*2 + 1] || _scenery_atlases[i])
        {
            _maybe_empty = false;
            return false;
        }
    }

    return true;
}

tile_atlas* chunk::ground_atlas_at(std::size_t i) const noexcept { return _ground_atlases[i].get(); }
tile_atlas* chunk::wall_atlas_at(std::size_t i) const noexcept { return _wall_atlases[i].get(); }

tile_ref chunk::operator[](std::size_t idx) noexcept { return { *this, std::uint8_t(idx) }; }
tile_proto chunk::operator[](std::size_t idx) const noexcept { return tile_proto(tile_ref { *const_cast<chunk*>(this), std::uint8_t(idx) }); }
tile_ref chunk::operator[](local_coords xy) noexcept { return operator[](xy.to_index()); }
tile_proto chunk::operator[](local_coords xy) const noexcept { return operator[](xy.to_index()); }

auto chunk::begin() noexcept -> iterator { return iterator { *this, 0 }; }
auto chunk::end() noexcept -> iterator { return iterator { *this, TILE_COUNT }; }
auto chunk::cbegin() const noexcept -> const_iterator { return const_iterator { *this, 0 }; }
auto chunk::cend() const noexcept -> const_iterator { return const_iterator { *this, TILE_COUNT }; }
auto chunk::begin() const noexcept -> const_iterator { return cbegin(); }
auto chunk::end() const noexcept -> const_iterator { return cend(); }

void chunk::mark_ground_modified() noexcept { _ground_modified = true; _pass_modified = true; }
void chunk::mark_walls_modified() noexcept { _walls_modified = true; _pass_modified = true; }
void chunk::mark_scenery_modified() noexcept { _scenery_modified = true; _pass_modified = true; }

void chunk::mark_modified() noexcept
{
    mark_ground_modified();
    mark_walls_modified();
    mark_scenery_modified();
}

static constexpr auto tile_size2us = Vector2us(iTILE_SIZE2);

static constexpr Vector2s tile_start(std::size_t k)
{
    const auto i = std::uint8_t(k);
    const local_coords coord{i};
    constexpr auto tile_size2s = Vector2s(tile_size2us), half = tile_size2s/2;
    return tile_size2s * Vector2s(coord.x, coord.y) - half;
}

auto chunk::ensure_passability() noexcept -> lqt&
{
    auto& qt = *_static_lqt;

    if (!_pass_modified)
        return qt;
    _pass_modified = false;

    qt.Clear();
    _lqt_bboxes.clear();
    _lqt_bboxes.reserve(32);

    constexpr auto whole_tile = [](std::size_t k, pass_mode p) constexpr -> bbox {
        auto start = tile_start(k);
        return { start[0], start[1], tile_size2us[0], tile_size2us[1], p };
    };

    constexpr auto wall_north = [](std::size_t k, pass_mode p) constexpr -> bbox {
        auto start = tile_start(k) - Vector2s(0, 1);
        return { start[0], start[1], tile_size2us[0], 2, p };
    };

    constexpr auto wall_west = [](std::size_t k, pass_mode p) constexpr -> bbox {
        auto start = tile_start(k) - Vector2s(1, 0);
        return { start[0], start[1], 2, tile_size2us[1], p };
    };

    for (std::size_t i = 0; i < TILE_COUNT; i++)
    {
        const auto tile = const_cast<chunk&>(*this)[i];
        if (auto s = tile.scenery())
            if (auto p = s.frame.passability; p != pass_mode::pass)
                _lqt_bboxes.push_back(whole_tile(i, p));
        if (auto atlas = tile.ground_atlas())
            if (auto p = atlas->pass_mode(pass_mode::pass); p != pass_mode::pass)
                _lqt_bboxes.push_back(whole_tile(i, p));
        if (auto atlas = tile.wall_north_atlas())
            if (auto p = atlas->pass_mode(pass_mode::blocked); p != pass_mode::pass)
                _lqt_bboxes.push_back(wall_north(i, p));
        if (auto atlas = tile.wall_west_atlas())
            if (auto p = atlas->pass_mode(pass_mode::blocked); p != pass_mode::pass)
                _lqt_bboxes.push_back(wall_west(i, p));
    }

    for (auto& bbox : _lqt_bboxes)
        qt.Insert(&bbox);

    return qt;
}

void chunk::bb_extractor::ExtractBoundingBox(const chunk::bbox* x, BB* bbox)
{
    *bbox = { x->left, x->top, std::int16_t(x->width), std::int16_t(x->height) };
}

chunk::chunk() noexcept : _static_lqt { std::make_unique<lqt>() } {}
chunk::~chunk() noexcept = default;
chunk::chunk(chunk&&) noexcept = default;
chunk& chunk::operator=(chunk&&) noexcept = default;

} // namespace floormat
