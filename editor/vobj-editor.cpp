#include "vobj-editor.hpp"
#include "loader/vobj-info.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/light.hpp"
#include "app.hpp"
#include <array>
#include <utility>
#include <Corrade/Containers/StringView.h>

namespace floormat {

StringView vobj_factory::name() const { return info().name; }
StringView vobj_factory::descr() const { return info().descr; }
std::shared_ptr<anim_atlas> vobj_factory::atlas() const { return info().atlas; }
vobj_factory::vobj_factory() = default;
vobj_factory::~vobj_factory() noexcept = default;

vobj_editor::vobj_editor() = default;
void vobj_editor::select_tile(const vobj_& type) { _selected = &type; }
void vobj_editor::clear_selection() { _selected = nullptr; }

auto vobj_editor::get_selected() const -> const vobj_*
{
    return _selected;
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
        const auto& es = c.entities();
start:  while (auto id = a.object_at_cursor())
        {
            for (auto i = es.size()-1; i != (size_t)-1; i--)
            {
                if (es[i]->id == id)
                {
                    c.remove_entity(i);
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

struct light_factory final : vobj_factory
{
    entity_type type() const override { return entity_type::light; }

    const vobj_info& info() const override
    {
        constexpr auto NAME = "light"_s;
        static const vobj_info& ret = loader.vobj(NAME);
        fm_debug_assert(ret.name == NAME);
        fm_debug_assert(ret.atlas != nullptr);
        return ret;
    }

    std::shared_ptr<entity> make(world& w, object_id id, global_coords pos) const override
    {
        auto ret = w.make_entity<light>(id, pos, light_proto{});
        return ret;
    }
};

auto vobj_editor::make_vobj_type_map() -> std::map<StringView, vobj_>
{
    constexpr auto add = [](auto& m, std::unique_ptr<vobj_factory>&& x) {
        StringView name = x->name(), descr = x->descr();
        m[name] = vobj_editor::vobj_{ name, descr, std::move(x) };
    };
    std::map<StringView, vobj_editor::vobj_> map;
    add(map, std::make_unique<light_factory>());
    return map;
}

} // namespace floormat
