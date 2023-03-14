#include "app.hpp"
#include "compat/format.hpp"
#include "inspect.hpp"
#include "main/clickable.hpp"
#include "floormat/main.hpp"
#include "src/world.hpp"
#include "src/anim-atlas.hpp"
#include "imgui-raii.hpp"
#include "chunk.inl"
#include "loader/loader.hpp"

namespace floormat {

using namespace floormat::imgui;

void app::draw_inspector()
{
    auto b = push_id("inspector");
    auto& w = M->world();

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
        auto& s = *e;
        chunk_coords ch = e->coord.chunk();
        local_coords pos = e->coord.local();
        auto& c = w[ch];

        char buf[128];
        snformat(buf, "i-{}-{}x{}-{}x{}"_cf, (int)target, ch.x, ch.y, (int)pos.x, (int)pos.y);

        auto b1 = push_id(buf);
        ImGui::SetNextWindowSize({300*dpi[0], 0});
        auto name = loader.strip_prefix(s.atlas->name());
        snformat(buf, "{} ({}x{} -> {}x{})"_cf, name, ch.x, ch.y, (int)pos.x, (int)pos.y);
        bool is_open = true;
        if (auto b2 = begin_window(buf, &is_open))
        {
            if (s.type == entity_type::scenery)
            {
                auto& s2 = static_cast<scenery&>(s);
                c.with_scenery_update(s, [&] { return entities::inspect_type(s2); });
            }
        }
        if (!is_open)
            inspectors.erase(inspectors.begin() + (int)i);
    }
}

} // namespace floormat
