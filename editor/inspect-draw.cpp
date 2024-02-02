#include "app.hpp"
#include "compat/format.hpp"
#include "inspect.hpp"
#include "main/clickable.hpp"
#include "floormat/main.hpp"
#include "src/world.hpp"
#include "src/object.hpp"
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
        entity_inspector_name(buf, sizeof buf, e.id);
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

} // namespace floormat
