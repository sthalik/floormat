#include "app.hpp"
#include "compat/format.hpp"
#include "inspect.hpp"
#include "main/clickable.hpp"
#include "floormat/main.hpp"
#include "src/world.hpp"
#include "src/anim-atlas.hpp"
#include "imgui-raii.hpp"
#include "loader/loader.hpp"

namespace floormat {

using namespace floormat::imgui;

void app::draw_inspector()
{
    auto b = push_id("inspector");

    constexpr auto max_inspectors = 4; // todo change later to 32
    if (auto size = inspectors.size(); size > max_inspectors)
    {
        auto end = inspectors.begin() + (std::ptrdiff_t)size - max_inspectors;
        inspectors.erase(inspectors.begin(), end);
    }

    const auto dpi = M->dpi_scale();

    for (auto i = inspectors.size()-1; i != -1_uz; i--)
    {
        auto [e, target] = inspectors[i];
        fm_debug_assert(e);
        auto& s = *e;
        chunk_coords ch = e->coord.chunk();
        local_coords pos = e->coord.local();

        char buf[32];
        snformat(buf, "inspector-{:08x}"_cf, s.id);

        auto b1 = push_id(buf);
        ImGui::SetNextWindowSize({300*dpi[0], 0});
        auto name = loader.strip_prefix(s.atlas->name());
        snformat(buf, "{} ({}x{} -> {}x{})"_cf, name, ch.x, ch.y, (int)pos.x, (int)pos.y);

        bool is_open = true;
        if (s.type == entity_type::scenery)
        {
            auto& s2 = static_cast<scenery&>(s);
            if (auto b2 = begin_window(buf, &is_open))
                entities::inspect_type(s2);
        }
        else
            is_open = false;
        if (!is_open)
            inspectors.erase(inspectors.begin() + (int)i);
    }
}

} // namespace floormat
