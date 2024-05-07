#include "critter-script.hpp"
//#include "raycast.hpp"
#include "critter.hpp"
#include "compat/failwith.hpp"
#include "point.hpp"
#include "critter.hpp"
#include "search-result.hpp"
#include "search-astar.hpp"
#include "entity/name-of.hpp"

namespace floormat {

namespace {

struct walk_script;
constexpr StringView script_name = name_of<walk_script>;
using ScriptPtr = Pointer<critter_script>;
using psr = path_search_result;

struct walk_script final : critter_script
{
    enum class walk_mode : uint8_t { none, line, path, };

    StringView name() const override;
    const void* id() const override;
    void on_init(const std::shared_ptr<critter>& c) override;
    void on_update(const std::shared_ptr<critter>& c, size_t& i, const Ns& dt) override;
    void on_destroy(const std::shared_ptr<critter>& c, script_destroy_reason reason) override;
    void delete_self() noexcept override;

    walk_script(point dest);
    walk_script(psr path);

private:
    point dest;
    psr path;
    uint32_t path_index = -1u;
    walk_mode mode = failwith<walk_mode>("walk_mode not set");
};

StringView walk_script::name() const { return "walk"_s; }
const void* walk_script::id() const { return &script_name; }
void walk_script::on_destroy(const std::shared_ptr<critter>& c, script_destroy_reason) { c->clear_auto_movement(); }
void walk_script::delete_self() noexcept { delete this; }

void walk_script::on_init(const std::shared_ptr<critter>& c)
{
    Debug{} << "| start walking from" << c->position() << "to" << dest;
    c->moves.AUTO = true;

    switch (mode)
    {
    case walk_mode::line:
        break;
    case walk_mode::path:
        fm_assert(!path.empty());
        dest = path.path().back();
        break;
    default:
        std::unreachable();
        fm_assert(false);
    }
}

walk_script::walk_script(point dest) : dest{dest}, mode{walk_mode::line} {}
walk_script::walk_script(psr path) : path{move(path)}, mode{walk_mode::path} { fm_assert(!path.empty()); }

bool walk_line(point dest, const std::shared_ptr<critter>& c, size_t& i, const Ns& dt)
{
    auto res = c->move_toward(i, dt, dest);
    return res.blocked || c->position() == dest;
}

bool walk_path(point dest, const path_search_result& path, const std::shared_ptr<critter>& c, size_t& i, const Ns& dt)
{
    return {};
}

void walk_script::on_update(const std::shared_ptr<critter>& c, size_t& i, const Ns& dt)
{
    auto& m = c->moves;
    if (c->maybe_stop_auto_movement())
        goto done;

    switch (mode)
    {
    case walk_mode::line:
        fm_assert(!path);
        if (walk_line(dest, c, i, dt))
            goto done;
        return;
    case walk_mode::path:
        if (walk_path(dest, path, c, i, dt))
            goto done;
        return;
    case walk_mode::none:
        break;
    }
    fm_assert(false);

done:
    //path = {};
    Debug{} << "  finished walking";
    c->clear_auto_movement();
    c->script.do_clear(c);
}

} // namespace

ScriptPtr critter_script::make_walk_script(point dest) { return ScriptPtr(new walk_script{dest}); }
ScriptPtr critter_script::make_walk_script(psr path)   { return ScriptPtr(new walk_script{move(path)}); }

} // namespace floormat
