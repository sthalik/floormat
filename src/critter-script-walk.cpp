#include "critter-script.hpp"
//#include "raycast.hpp"
#include "point.hpp"
#include "critter.hpp"
#include "search-result.hpp"
#include "search-astar.hpp"
#include "entity/name-of.hpp"
#include <cr/Optional.h>

namespace floormat {

using walk_mode = critter_script::walk_mode;

namespace {

bool walk_line(point dest, const std::shared_ptr<critter>& c, size_t& i, const Ns& dt)
{
    Debug{} << "move from" << c->position() << "to" << dest;
    auto res = c->move_toward(i, dt, dest);
    DBG_nospace << "blocked:" << res.blocked << " moved:" << res.moved;
    return res.blocked || c->position() == dest;
}

struct walk_script final : critter_script
{
    struct line_tag_t {};
    struct path_tag_t {};

    const void* type_id() const override;
    void on_init(const std::shared_ptr<critter>& c) override;
    void on_update(const std::shared_ptr<critter>& c, size_t& i, const Ns& dt) override;
    void on_destroy(const std::shared_ptr<critter>& c, script_destroy_reason reason) override;
    void delete_self() noexcept override;

    walk_script(point dest, line_tag_t);
    //walk_script(point dest, path_search_result path, path_tag_t);

private:
    point dest;
    Optional<path_search_result> path;
    walk_mode mode = walk_mode::none;
};

walk_script::walk_script(point dest, line_tag_t) :
    dest{dest}, mode{walk_mode::line}
{
}

void walk_script::on_init(const std::shared_ptr<critter>& c)
{
    c->movement.AUTO = true;
}

void walk_script::on_update(const std::shared_ptr<critter>& c, size_t& i, const Ns& dt)
{
    auto& m = c->movement;

    if (m.L | m.R | m.U | m.D)
        m.AUTO = false;

    if (!m.AUTO)
        goto done;

    switch (mode)
    {
    case walk_mode::none:
        fm_abort("bad walk_mode 'none'");
    case walk_mode::line:
        fm_assert(!path);
        if (walk_line(dest, c, i, dt))
            goto done;
        return;
    case walk_mode::path:
        fm_abort("todo!");
        //break;
    }
    std::unreachable();
    fm_assert(false);

done:
    Debug{} << "  finished walking";
    c->script.do_clear(c);
}

constexpr StringView script_name = name_of<walk_script>;

const void* walk_script::type_id() const
{
    return &script_name;
}

void walk_script::on_destroy(const std::shared_ptr<critter>& c, script_destroy_reason reason)
{
}

void walk_script::delete_self() noexcept
{
    delete this;
}

} // namespace

Pointer<critter_script> critter_script::make_walk_script(point dest, const path_search_result& path, walk_mode mode)
{
    switch (mode)
    {
    case walk_mode::line:
        fm_assert(path.empty());
        return Pointer<critter_script>(new walk_script{dest, walk_script::line_tag_t{}});
    case walk_mode::path:
        fm_abort("todo!");
        fm_assert(!path.empty());
    case walk_mode::none:
        break;
    }
    fm_abort("bad walk_mode %d", (int)mode);
}

} // namespace floormat
