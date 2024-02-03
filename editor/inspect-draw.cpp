#include "app.hpp"
#include "compat/format.hpp"
#include "inspect.hpp"
#include "main/clickable.hpp"
#include "floormat/main.hpp"
#include "src/world.hpp"
#include "src/object.hpp"
#include "src/scenery.hpp"
#include "src/critter.hpp"
#include "src/light.hpp"
#include "src/anim-atlas.hpp"
#include "imgui-raii.hpp"
#include "loader/loader.hpp"

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
        auto e_ = w.find_object(id);
        if (!e_)
        {
            erase_inspector((unsigned)i);
            continue;
        }
        auto& e = *e_;

        char buf[256], buf2[32], buf3[128];
        ImGui::SetNextWindowSize({375*dpi[0], 0});
#if 0
        auto name = loader.strip_prefix(e.atlas->name());
        chunk_coords ch = e.coord.chunk();
        local_coords pos = e.coord.local();
auto z = e.coord.z();
        if (z == 0)
            snformat(buf, "{} ({}x{} -> {}x{})###inspector-{:08x}"_cf, name, ch.x, ch.y, (int)pos.x, (int)pos.y, e.id);
        else
            snformat(buf, "{} ({}x{}:{} -> {}x{})###inspector-{:08x}"_cf, name, ch.x, ch.y, (int)z, (int)pos.x, (int)pos.y, e.id);
#else
        entity_inspector_name(buf2, sizeof buf2, e.id);
        entity_friendly_name(buf3, sizeof buf3, e);
        std::snprintf(buf, std::size(buf), "%s###%s", buf3, buf2);
#endif

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

void app::entity_inspector_name(char* buf, size_t len, object_id id)
{
    constexpr auto min_len = sizeof "inspector-" + 8;
    fm_debug_assert(len >= min_len);
    auto result = fmt::format_to_n(buf, len, "inspector-{:08x}"_cf, id);
    fm_assert(result.size < len);
    buf[result.size] = '\0';
}

void app::entity_friendly_name(char* buf, size_t len, const object& obj)
{
    switch (obj.type())
    {
    default:
        std::snprintf(buf, len, "(unknown?)");
        break;
    case object_type::critter: {
        const auto& c = static_cast<const critter&>(obj);
        std::snprintf(buf, len, "critter %s", c.name.data());
        break;
    }
    case object_type::light: {
        const auto& L = static_cast<const light&>(obj);
        const char* type;
        switch (L.falloff)
        {
        case light_falloff::constant: type = "constant"; break;
        case light_falloff::linear: type = "linear"; break;
        case light_falloff::quadratic: type = "quadratic"; break;
        default: type = "(unknown?)"; break;
        }
        std::snprintf(buf, len, "light #%hhx%hhx%hhx%hhx %s:%.2f",
                      L.color.r(), L.color.g(), L.color.b(), L.color.a(),
                      type,
                      L.falloff == light_falloff::constant ? 0. : (double)L.max_distance);
        break;
    }
    case object_type::scenery: {
        const auto& sc = static_cast<const scenery&>(obj);
        switch (sc.scenery_type())
        {
        default:
            std::snprintf(buf, len, "(unknown?)");
            break;
        case scenery_type::door:
            std::snprintf(buf, len, "door");
            break;
        case scenery_type::generic:
            std::snprintf(buf, len, "scenery %s", sc.atlas->name().data());
            break;
        }
    }
    }
}

} // namespace floormat
