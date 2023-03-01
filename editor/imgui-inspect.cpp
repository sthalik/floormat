#include "app.hpp"
#include "inspect.hpp"
#include "main/clickable.hpp"
#include "floormat/main.hpp"
#include "src/world.hpp"
#include "imgui-raii.hpp"
#include "chunk.inl"

namespace floormat {

using namespace floormat::imgui;

void app::draw_inspector()
{
    auto b = push_id("inspector");
    auto& w = M->world();
    if (cursor.pixel)
        if (const auto* sc = find_clickable_scenery(cursor.pixel))
            inspected_scenery = {InPlaceInit, sc->chunk, sc->pos};
    if (inspected_scenery)
    {
        auto [c, t] = w[*inspected_scenery];
        if (auto s = t.scenery())
        {
            char buf[32]; std::snprintf(buf, sizeof buf, "i_0x%p", (void*)&s);
            auto b = push_id(buf);
            auto dpi = M->dpi_scale();
            ImGui::SetNextWindowSize({300*dpi[0], 0});
            auto b2 = begin_window("inspector"_s);
            c.with_scenery_bbox_update(s.index(), [&] { entities::inspect_type(s); });
        }
    }
}

} // namespace floormat
