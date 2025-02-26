#include "vobj-editor.hpp"
#include "src/world.hpp"
#include "src/light.hpp"
#include "src/hole.hpp"
#include "loader/loader.hpp"
#include "loader/vobj-cell.hpp"
#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include <Corrade/Containers/StringView.h>

namespace floormat {

StringView vobj_factory::name() const { return info().name; }
StringView vobj_factory::descr() const { return info().descr; }
bptr<anim_atlas> vobj_factory::atlas() const { return info().atlas; }
vobj_factory::vobj_factory() = default;
vobj_factory::~vobj_factory() noexcept = default;

vobj_editor::vobj_editor() = default;
void vobj_editor::select_tile(const vobj_& type) { _selected = &type; }
void vobj_editor::clear_selection() { _selected = nullptr; }

auto vobj_editor::get_selected() const -> const vobj_*
{
    if (_selected)
        return _selected;
    else
        return {};
}

auto vobj_editor::get_type(StringView name) -> const vobj_*
{
    auto it = _types.find(name);
    if (it != _types.cend())
        return &it->second;
    else
    {
        fm_warn_once("invalid vobj type '%s'", name.data());
        return nullptr;
    }
}

bool vobj_editor::is_item_selected(const vobj_& x) const { return _selected == &x; }
bool vobj_editor::is_anything_selected() const { return _selected != nullptr; }

void vobj_editor::place_tile(world& w, global_coords pos, const vobj_* x, struct app& a)
{
    if (!x)
    {
        auto [c, t] = w[pos];
start:
        const auto& es = c.objects();
        while (auto id = a.get_object_colliding_with_cursor())
        {
            for (auto i = (int)(es.size()-1); i >= 0; i--)
            {
                auto eʹ = es[i];
                auto& e = *eʹ;
                if (e.id == id && eʹ->is_virtual())
                {
                    e.destroy_script_pre(eʹ, script_destroy_reason::kill);
                    c.remove_object((unsigned)i);
                    e.destroy_script_post();
                    eʹ.destroy();
                    goto start;
                }
            }
            break;
        }
    }
    else
        x->factory->make(w, w.make_id(), pos);
}

#if defined __clang__ || defined __CLION_IDE__
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

namespace {

struct light_factory final : vobj_factory
{
    object_type type() const override;
    const vobj_cell& info() const override;
    bptr<object> make(world& w, object_id id, global_coords pos) const override;
};

object_type light_factory::type() const { return object_type::light; }

const vobj_cell& light_factory::info() const
{
    constexpr auto NAME = "light"_s;
    static const vobj_cell& ret = loader.vobj(NAME);
    fm_debug_assert(ret.name == NAME);
    fm_debug_assert(ret.atlas != nullptr);
    return ret;
}

bptr<object> light_factory::make(world& w, object_id id, global_coords pos) const
{
    auto ret = w.make_object<light>(id, pos, light_proto{});
    return ret;
}

struct hole_factory final : vobj_factory
{
    object_type type() const override;
    const vobj_cell& info() const override;
    bptr<object> make(world& w, object_id id, global_coords pos) const override;
};

object_type hole_factory::type() const { return object_type::hole; }

const vobj_cell& hole_factory::info() const
{
    constexpr auto NAME = "hole"_s;
    static const vobj_cell& ret = loader.vobj(NAME);
    fm_debug_assert(ret.name == NAME);
    fm_debug_assert(ret.atlas != nullptr);
    return ret;
}

bptr<object> hole_factory::make(world& w, object_id id, global_coords pos) const
{
    auto ret = w.make_object<hole>(id, pos, hole_proto{});
    return ret;
}

} // namespace

auto vobj_editor::make_vobj_type_map() -> std::map<String, vobj_>
{
    constexpr auto add = [](auto& m, Pointer<vobj_factory>&& x) {
        StringView name = x->name(), descr = x->descr();
        m[name] = vobj_editor::vobj_{ name, descr, move(x) };
    };
    std::map<String, vobj_> map;
    add(map, Pointer<light_factory>{InPlace});
    add(map, Pointer<hole_factory>{InPlace});
    return map;
}

} // namespace floormat
