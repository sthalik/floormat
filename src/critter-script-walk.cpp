#include "compat/nanosecond.hpp"
#include "critter-script.hpp"
#include "critter-walk.hpp"
#include "point.inl"
#include "critter.hpp"
#include "search-result.hpp"
#include "search-astar.hpp"
#include "raycast-line.hpp"
#include "rotation.inl"
#include "sweep-aabb.hpp"
#include "compat/function-view.hpp"
#include "entity/name-of.hpp"
#include <utility>

namespace floormat {

critter_walk::critter_walk() = default;

critter_walk::critter_walk(point dest) : _dest{dest}, mode{walk_mode::line} {}

critter_walk::critter_walk(path_search_result pathʹ) :
    path{move(pathʹ)},
    path_index{0},
    mode{walk_mode::path}
{
    fm_assert(!path.empty());
    _dest = path.path().back();
    if (std::as_const(path).smoothings().size() >= 2)
    {
        mode = walk_mode::smoothed;
        path_index = 1;
    }
}

point critter_walk::dest() const { return _dest; }
bool critter_walk::is_line() const { return mode == walk_mode::line; }

bool critter_walk::is_remaining_path_unblocked(critter& c) const
{
    if (mode != walk_mode::smoothed)
        return true;
    const auto sp = path.smoothings();
    const auto self_id = c.id;
    const auto pred = [self_id](const class chunk&, collision_data x) {
        return x.id == self_id ? path_search_continue::pass : path_search_continue::blocked;
    };
    const auto pos = c.position();
    auto r = c.r;
    for (auto j = path_index; j < sp.size(); j++)
    {
        const auto from = sp[j-1].pt, to = sp[j].pt;
        const auto ch = from.chunk3();
        const auto origin = point{ch, {}, {}};
        const auto line = Raycast::bresenham_line{from - origin, to - origin};
        auto k = 0u;
        if (j == path_index)
        {
            k = pos.chunk3().z == ch.z ? step_at(line, pos - origin) : -1u;
            if (k == -1u)
                return false;
        }
        if (k == line.steps())
            continue;
        const auto new_r = nearest_rotation(to - from);
        if (new_r != r)
        {
            if (j == path_index)
            {
                if (!c.can_rotate(new_r))
                    return false;
            }
            else if (!c.can_rotate(from.coord(), new_r, r, from.offset(), rotate_point(c.bbox_offset, c.r, r),
                                   rotate_size(c.bbox_size, c.r, r)))
                return false;
        }
        const auto [bb_off, size] = rotate_bbox(c.bbox_offset, c.bbox_size, c.r, new_r);
        const auto box_line = Raycast::bresenham_line{from - origin + Vector2i(bb_off), to - origin + Vector2i(bb_off)};
        if (find_swept_collider(c.world(), ch, box_line, k, line.steps(), Vector2ui(size), pred).has_collider)
            return false;
        r = new_r;
    }
    return true;
}

auto critter_walk::update(const bptr<critter>& c, usize& i, const Ns& dt) -> walk_result
{
    fm_assert(mode != walk_mode::none);
    if (mode == walk_mode::line)
    {
        fm_assert(!path);
        return walk_line(c, i, dt);
    }
    if (mode == walk_mode::path)
        return walk_path(c, i, dt);
    return walk_smoothed(c, i, dt);
}

auto critter_walk::walk_line(const bptr<critter>& c, usize& i, const Ns& dtʹ) -> walk_result
{
    auto dt = dtʹ;
    auto res = c->move_toward(i, dt, _dest);
    if (res.blocked)
        return walk_result::blocked;
    return c->position() == _dest ? walk_result::arrived : walk_result::none;
}

auto critter_walk::walk_path(const bptr<critter>& c, usize& i, const Ns& dtʹ) -> walk_result
{
    auto dt = dtʹ;
    while (dt != Ns{})
    {
        while (path_index < path.size() && c->position() == path.path()[path_index])
            path_index++;
        if (path_index == path.size())
            return walk_result::arrived;
        point cur = path.path()[path_index];
        auto ret = c->move_toward(i, dt, cur);
        if (ret.blocked)
            return walk_result::blocked;
        if (!ret.moved)
            return walk_result::none;
    }
    return walk_result::none;
}

auto critter_walk::walk_smoothed(const bptr<critter>& c, usize& i, const Ns& dtʹ) -> walk_result
{
    auto dt = dtʹ;
    const auto sp = std::as_const(path).smoothings();
    if (path_index == sp.size())
        return walk_result::arrived;
    for (;;)
    {
        const auto to = sp[path_index].pt;
        const auto ret = c->move_toward(i, dt, sp[path_index-1].pt, to);
        if (ret.displaced)
            return walk_result::displaced;
        if (ret.blocked)
            return walk_result::blocked;
        if (c->position() != to)
            return walk_result::none;
        if (++path_index == sp.size())
            return walk_result::arrived;
        if (dt == Ns{})
            return walk_result::none;
    }
}

namespace {

struct walk_script;
constexpr StringView script_name = name_of<walk_script>;
using ScriptPtr = Pointer<critter_script>;
using psr = path_search_result;

struct walk_script final : critter_script
{
    using walk_result = critter::walk_result;

    StringView name() const override;
    const void* id() const override;
    void on_init(const bptr<critter>& c) override;
    void on_update(const bptr<critter>& c, usize& i, const Ns& dt) override;
    void on_destroy(const bptr<critter>& c, script_destroy_reason reason) override;
    void delete_self() noexcept override;

    explicit walk_script(point dest);
    explicit walk_script(psr path);

private:
    critter_walk walk;
};

StringView walk_script::name() const { return "walk"_s; }
const void* walk_script::id() const { return &script_name; }
void walk_script::on_destroy(const bptr<critter>& c, script_destroy_reason) { c->clear_auto_movement(); }
void walk_script::delete_self() noexcept { delete this; }

void walk_script::on_init(const bptr<critter>& c)
{
    //Debug{} << "| start walking from" << c->position() << "to" << walk.dest();
    c->moves.AUTO = true;
    c->last_walk = walk_result::none;
}

void walk_script::on_update(const bptr<critter>& c, usize& i, const Ns& dt)
{
    auto ret = walk_result::none;
    if (c->maybe_stop_auto_movement())
        goto done;

    ret = walk.update(c, i, dt);
    if (ret == walk_result::none)
        return;

done:
    //Debug{} << "  finished walking";
    c->last_walk = ret;
    c->clear_auto_movement();
    c->script.do_clear(c);
}

walk_script::walk_script(point dest) : walk{dest} {}
walk_script::walk_script(psr path) : walk{move(path)} {}

} // namespace

ScriptPtr critter_script::make_walk_script(point dest) { return ScriptPtr(new walk_script{dest}); }
ScriptPtr critter_script::make_walk_script(psr path)   { return ScriptPtr(new walk_script{move(path)}); }

} // namespace floormat
