#include "app.hpp"
#include "compat/array-size.hpp"
#include "compat/format.hpp"
#include "inspect.hpp"
#include "floormat/main.hpp"
#include "src/world.hpp"
#include "src/object.hpp"
#include "src/scenery.hpp"
#include "src/critter.hpp"
#include "src/light.hpp"
#include "src/anim-atlas.hpp"
#include "imgui-raii.hpp"

namespace floormat {

using namespace floormat::imgui;

void app::draw_inspector()
{
    auto b = push_id("inspector");

    const auto dpi = M->dpi_scale();
    auto& w = M->world();

    for (auto i = (int)(inspectors.size()-1); i >= 0; i--)
    {
        auto [id, target] = inspectors[i];
        auto eʹ = w.find_object(id);
        if (!eʹ)
        {
            erase_inspector((unsigned)i);
            continue;
        }
        auto& e = *eʹ;

        char buf2[10], buf3[128], buf[sizeof buf2 + sizeof buf3 - 1];
        ImGui::SetNextWindowSize({375*dpi[0], 0});
        entity_inspector_name(buf2, e.id);
        entity_friendly_name(buf3, sizeof buf3, e);
        std::snprintf(buf, sizeof buf, "%s###%s", buf3, buf2);

        bool is_open = true;
        if (auto b2 = begin_window(buf, &is_open))
        {
            bool ret = entities::inspect_object_subtype(e);
            (void)ret;
        }
        if (!is_open)
            erase_inspector((unsigned)i);
    }
}

void app::entity_inspector_name(char(&buf)[10], object_id id)
{
    auto result = fmt::format_to_n(buf, array_size(buf), "@{:08x}"_cf, id);
    fm_assert(result.size == 9);
    buf[array_size(buf)-1] = '\0';
}

void app::entity_friendly_name(char* buf, size_t len, const object& obj)
{
    switch (obj.type())
    {
    case object_type::none:
    case object_type::COUNT:
        break;
    case object_type::hole:
        std::snprintf(buf, len, "%s", "hole");
        return;
    case object_type::critter: {
        const auto& c = static_cast<const critter&>(obj);
        std::snprintf(buf, len, "critter %s", c.name.data());
        return;
    }
    case object_type::light: {
        const auto& L = static_cast<const light&>(obj);
        const char* type = "(unknown?)";
        switch (L.falloff)
        {
        case light_falloff::COUNT: break;
        case light_falloff::constant: type = "constant"; break;
        case light_falloff::linear: type = "linear"; break;
        case light_falloff::quadratic: type = "quadratic"; break;
        }
        std::snprintf(buf, len, "light #%hhx%hhx%hhx%hhx %s:%.2f",
                      L.color.r(), L.color.g(), L.color.b(), L.color.a(),
                      type,
                      L.falloff == light_falloff::constant ? 0. : (double)L.max_distance);
        return;
    }
    case object_type::scenery: {
        const auto& sc = static_cast<const scenery&>(obj);
        switch (sc.scenery_type())
        {
        case scenery_type::none:
        case scenery_type::COUNT:
            break;
        case scenery_type::door:
            std::snprintf(buf, len, "door");
            return;
        case scenery_type::generic:
            std::snprintf(buf, len, "scenery %s", sc.atlas->name().data());
            return;
        }
        std::snprintf(buf, len, "(scenery?)");
        return;
    }
    }
    std::snprintf(buf, len, "(unknown?)");
}

} // namespace floormat
