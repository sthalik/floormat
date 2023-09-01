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
        auto end = inspectors.begin() + (ptrdiff_t)size - max_inspectors;
        inspectors.erase(inspectors.begin(), end);
        fm_assert(inspectors.size() <= max_inspectors);
    }

    const auto dpi = M->dpi_scale();
    auto& w = M->world();

    for (auto i = inspectors.size()-1; i != -1uz; i--)
    {
        auto [id, target] = inspectors[i];
        auto e_ = w.find_object(id);
        if (!e_)
        {
            inspectors.erase(inspectors.begin() + ptrdiff_t(i));
            continue;
        }
        auto& e = *e_;

        char buf[32];
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
        snformat(buf, "inspector-{:08x}"_cf, e.id);
#endif

        bool is_open = true;
        if (auto b2 = begin_window(buf, &is_open))
        {
            bool ret = entities::inspect_object_subtype(e);
            (void)ret;
        }
        if (!is_open)
            inspectors.erase(inspectors.begin() + (ptrdiff_t)i);
    }
}

} // namespace floormat
