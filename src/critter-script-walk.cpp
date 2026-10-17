#include "critter-script.hpp"
#include "compat/failwith.hpp"
#include "point.inl"
#include "nanosecond.hpp"
#include "critter.hpp"
#include "search-result.hpp"
#include "search-astar.hpp"
#include "entity/name-of.hpp"
#include <utility>

namespace floormat {

namespace {

struct walk_script;
constexpr StringView script_name = name_of<walk_script>;
using ScriptPtr = Pointer<critter_script>;
using psr = path_search_result;

struct walk_script final : critter_script
{
    enum class walk_mode : uint8_t { none, line, path, smoothed, };
    using walk_result = critter::walk_result;

    StringView name() const override;
    const void* id() const override;
    void on_init(const bptr<critter>& c) override;
    void on_update(const bptr<critter>& c, size_t& i, const Ns& dt) override;
    void on_destroy(const bptr<critter>& c, script_destroy_reason reason) override;
    void delete_self() noexcept override;

    explicit walk_script(point dest);
    explicit walk_script(psr path);

    walk_result walk_line(const bptr<critter>& c, size_t& i, const Ns& dt);
    walk_result walk_path(const bptr<critter>& c, size_t& i, const Ns& dt);
    walk_result walk_smoothed(const bptr<critter>& c, size_t& i, const Ns& dt);

private:
    point dest;
    psr path;
    uint32_t path_index = -1u;
    walk_mode mode = failwith<walk_mode>("walk_mode not set");
};

StringView walk_script::name() const { return "walk"_s; }
const void* walk_script::id() const { return &script_name; }
void walk_script::on_destroy(const bptr<critter>& c, script_destroy_reason) { c->clear_auto_movement(); }
void walk_script::delete_self() noexcept { delete this; }

void walk_script::on_init(const bptr<critter>& c)
{
    //Debug{} << "| start walking from" << c->position() << "to" << dest;
    c->moves.AUTO = true;
    c->last_walk = walk_result::none;

    switch (mode)
    {
    case walk_mode::line:
        break;
    case walk_mode::path:
    case walk_mode::smoothed:
        fm_assert(!path.empty());
        dest = path.path().back();
        break;
    default:
        fm_assert(false);
    }
}

void walk_script::on_update(const bptr<critter>& c, size_t& i, const Ns& dt)
{
    auto ret = walk_result::none;
    if (c->maybe_stop_auto_movement())
        goto done;

    switch (mode)
    {
    case walk_mode::line:
        fm_assert(!path);
        ret = walk_line(c, i, dt);
        break;
    case walk_mode::path:
        ret = walk_path(c, i, dt);
        break;
    case walk_mode::smoothed:
        ret = walk_smoothed(c, i, dt);
        break;
    case walk_mode::none:
        fm_assert(false);
    }
    if (ret == walk_result::none)
        return;

done:
    //path = {};
    //Debug{} << "  finished walking";
    c->last_walk = ret;
    c->clear_auto_movement();
    c->script.do_clear(c);
}

walk_script::walk_script(point dest) : dest{dest}, mode{walk_mode::line} {}

walk_script::walk_script(psr pathʹ) :
    dest{pathʹ.path().back()},
    path{move(pathʹ)},
    path_index{0},
    mode{walk_mode::path}
{
    fm_assert(!path.empty());
    if (std::as_const(path).smoothings().size() >= 2)
    {
        mode = walk_mode::smoothed;
        path_index = 1;
    }
}

auto walk_script::walk_line(const bptr<critter>& c, size_t& i, const Ns& dtʹ) -> walk_result
{
    auto dt = dtʹ;
    auto res = c->move_toward(i, dt, dest);
    if (res.blocked)
        return walk_result::blocked;
    return c->position() == dest ? walk_result::arrived : walk_result::none;
}

auto walk_script::walk_path(const bptr<critter>& c, size_t& i, const Ns& dtʹ) -> walk_result
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

auto walk_script::walk_smoothed(const bptr<critter>& c, size_t& i, const Ns& dtʹ) -> walk_result
{
    auto dt = dtʹ;
    const auto sp = std::as_const(path).smoothings();
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

} // namespace

ScriptPtr critter_script::make_walk_script(point dest) { return ScriptPtr(new walk_script{dest}); }
ScriptPtr critter_script::make_walk_script(psr path)   { return ScriptPtr(new walk_script{move(path)}); }

} // namespace floormat
