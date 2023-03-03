#include "chunk.hpp"
#include "src/tile-atlas.hpp"
#include <Magnum/GL/Context.h>

namespace floormat {

namespace {

std::size_t _reload_no_ = 0;

bool is_log_quiet()
{
    using GLCCF = GL::Implementation::ContextConfigurationFlag;
    auto flags = GL::Context::current().configurationFlags();
    return !!(flags & GLCCF::QuietLog);
}

} // namespace

bool chunk::empty(bool force) const noexcept
{
    if (!force && !_maybe_empty)
        return false;
    for (auto i = 0_uz; i < TILE_COUNT; i++)
        if (_ground_atlases[i] || _wall_atlases[i*2 + 0] || _wall_atlases[i*2 + 1] || _scenery_atlases[i])
            return _maybe_empty = false;
    return true;
}

tile_atlas* chunk::ground_atlas_at(std::size_t i) const noexcept { return _ground_atlases[i].get(); }
tile_atlas* chunk::wall_atlas_at(std::size_t i) const noexcept { return _wall_atlases[i].get(); }

std::shared_ptr<anim_atlas>& chunk::scenery_atlas_at(std::size_t i) noexcept { return _scenery_atlases[i]; }
scenery& chunk::scenery_at(std::size_t i) noexcept { return _scenery_variants[i]; }

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

void chunk::mark_ground_modified() noexcept
{
    if (!_ground_modified && !is_log_quiet())
        fm_debug("ground reload %zu", ++_reload_no_);
    _ground_modified = true;
    mark_passability_modified();
}

// todo this can use _replace_bbox too
void chunk::mark_walls_modified() noexcept
{
    if (!_walls_modified && !is_log_quiet())
        fm_debug("wall reload %zu", ++_reload_no_);
    _walls_modified = true;
    mark_passability_modified();
}

void chunk::mark_scenery_modified(bool collision_too) noexcept // todo remove bool
{
    if (!_scenery_modified && !is_log_quiet())
        fm_debug("scenery reload %zu", ++_reload_no_);
    _scenery_modified = true;
    if (collision_too)
        mark_passability_modified();
}

void chunk::mark_passability_modified() noexcept
{
    if (!_pass_modified && !is_log_quiet())
        fm_debug("pass reload %zu", ++_reload_no_);
    _pass_modified = true;
}

bool chunk::is_passability_modified() const noexcept { return _pass_modified; }
bool chunk::is_scenery_modified() const noexcept { return _scenery_modified; }

void chunk::mark_modified() noexcept
{
    mark_ground_modified();
    mark_walls_modified();
    mark_scenery_modified();
}

chunk::chunk() noexcept = default;
chunk::~chunk() noexcept = default;

chunk::chunk(chunk&&) noexcept = default;
chunk& chunk::operator=(chunk&&) noexcept = default;

bool chunk::bbox::operator==(const bbox& other) const noexcept = default;

} // namespace floormat
