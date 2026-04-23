#include "anim-traits.hpp"
#include "atlas-loader-storage.hpp"
#include "anim-cell.hpp"
#include "src/anim-atlas.hpp"
#include "src/tile-defs.hpp"
#include "loader.hpp"
#include "src/sprite-atlas.hpp"
#include "src/sprite-constants.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/anim.hpp"
#include "compat/exception.hpp"
#include <cr/StringView.h>
#include <cr/GrowableArray.h>
#include <cr/StridedArrayView.h>
#include <cr/Pointer.h>
#include <cr/Optional.h>
#include <mg/ImageData.h>
#include <mg/ImageView.h>
#include <Magnum/PixelStorage.h>

namespace floormat::loader_detail {

using anim_traits = atlas_loader_traits<anim_atlas>;
StringView anim_traits::loader_name() { return "anim_atlas"_s; }
auto anim_traits::atlas_of(const Cell& x) -> const bptr<Atlas>& { return x.atlas; }
auto anim_traits::atlas_of(Cell& x) -> bptr<Atlas>& { return x.atlas; }
StringView anim_traits::name_of(const Cell& x) { return x.name; }
String& anim_traits::name_of(Cell& x) { return x.name; }

void anim_traits::atlas_list(Storage& s)
{
    fm_debug_assert(s.name_map.empty());
    s.cell_array = {};
    arrayReserve(s.cell_array, 16);
    s.name_map[loader.INVALID] = -1uz;
}

auto anim_traits::make_invalid_atlas(Storage& s) -> Cell
{
    fm_debug_assert(!s.invalid_atlas);

    constexpr auto size = Vector2ui{tile_size_xy/2};
    constexpr auto ground = Vector2i(size/2);

    auto frame = anim_frame {
        .ground = ground,
        .offset = {},
        .size = size,
    };
    auto groups = Array<anim_group>{ValueInit, 1};
    groups[0] = anim_group {
        .name = "n"_s,
        .frames = array({ frame }),
    };
    auto def = anim_def {
        .object_name = loader.INVALID,
        .anim_name = loader.INVALID,
        .groups = move(groups),
        .pixel_size = size,
        .scale = anim_scale::fixed{size.x(), true},
        .nframes = 1,
    };
    auto err_tex = loader.make_error_texture(size);

    // Allocate the single frame in the shared sprite atlas so render-path
    // lookups succeed for any object that ended up using this placeholder
    // (e.g. missing asset with loader_policy::warn). Must happen before the
    // atlas ctor consumes `def`. Same pattern as loader/vobj.cpp.
    {
        const auto px_size = (size_t)err_tex.pixels().size()[2];
        const auto row_stride = (size_t)size.x() * px_size;
        const auto* src_base = (const char*)err_tex.pixels().data();
        PixelStorage storage;
        storage.setRowLength((Int)size.x());
        storage.setAlignment(1);
        const ImageView2D view{storage, err_tex.format(),
                               {(Int)size.x(), (Int)size.y()},
                               ArrayView<const void>{src_base, (size_t)size.y() * row_stride}};
        auto sp = loader.atlas().add(view);
        def.groups[0].sprites = Array<const SpriteAtlas::Sprite*>{ValueInit, 1};
        def.groups[0].sprites[0] = sp.raw();
    }

    auto atlas = bptr<class anim_atlas>{InPlace, loader.INVALID, err_tex, move(def)};

    auto info = anim_cell {
        .atlas = atlas,
        .name = loader.INVALID,
    };
    return info;
}

auto anim_traits::make_atlas(StringView name, const Cell&) -> bptr<Atlas>
{
    char buf[fm_FILENAME_MAX];
    auto json_path = loader.make_atlas_path(buf, {}, name, ".json"_s);
    auto anim_info = json_helper::from_json<struct anim_def>(json_path);

    for (anim_group& group : anim_info.groups)
    {
        if (!group.mirror_from.isEmpty())
        {
            const auto *begin = anim_info.groups.data(), *end = begin + anim_info.groups.size();
            const auto* it = std::find_if(begin, end, [&](const anim_group& x) { return x.name == group.mirror_from; });
            if (it == end)
                fm_throw("can't find group '{}' to mirror from '{}'"_cf, group.mirror_from, group.name);
            group.frames = array(ArrayView<const anim_frame>{it->frames.data(), it->frames.size()});
            for (anim_frame& f : group.frames)
                f.ground = Vector2i((Int)f.size[0] - f.ground[0], f.ground[1]);
        }
    }

    auto tex = loader.texture(""_s, name);

    fm_soft_assert(!anim_info.object_name.isEmpty());
    fm_soft_assert(anim_info.pixel_size.product() > 0);
    fm_soft_assert(!anim_info.groups.isEmpty());
    fm_soft_assert(anim_info.nframes > 0);
    fm_soft_assert(anim_info.nframes == 1 || anim_info.fps > 0);
    const auto size = tex.pixels().size();
    const auto width = size[1], height = size[0];
    fm_soft_assert(anim_info.pixel_size[0] == width && anim_info.pixel_size[1] == height);

    {
        const auto format = tex.format();
        const auto full_width  = (uint32_t)size[1];
        const auto full_height = (uint32_t)size[0];

        for (anim_group& g : anim_info.groups)
        {
            if (!g.mirror_from.isEmpty())
                continue;
            g.sprites = Array<const SpriteAtlas::Sprite*>{ValueInit, g.frames.size()};
            for (uint32_t fi = 0; fi < g.frames.size(); fi++)
            {
                const anim_frame& f = g.frames[fi];
                const uint32_t fw = (uint32_t)f.size.x();
                const uint32_t fh = (uint32_t)f.size.y();
                fm_soft_assert(fw > 0 && fh > 0);
                fm_soft_assert(fw <= SpriteAtlas::max_texture_xy && fh <= SpriteAtlas::max_texture_xy);
                const uint32_t mem_y_start = full_height - (uint32_t)f.offset.y() - fh;
                PixelStorage sub_storage = tex.storage();
                sub_storage.setRowLength((Int)full_width)
                           .setSkip({(Int)f.offset.x(), (Int)mem_y_start, 0});
                const ImageView2D view{sub_storage, format,
                                       {(Int)fw, (Int)fh}, tex.data()};
                auto sp = loader.atlas().add(view);
                g.sprites[fi] = sp.raw();
            }
        }

        for (anim_group& g : anim_info.groups)
        {
            if (g.mirror_from.isEmpty())
                continue;
            const auto* begin = anim_info.groups.data(), *end = begin + anim_info.groups.size();
            const auto* src = std::find_if(begin, end, [&](const anim_group& x) { return x.name == g.mirror_from; });
            fm_debug_assert(src != end);
            g.sprites = Array<const SpriteAtlas::Sprite*>{ValueInit, g.frames.size()};
            for (uint32_t fi = 0; fi < g.frames.size(); fi++)
            {
                const auto* s = src->sprites[fi];
                if (!s) continue;
                g.sprites[fi] = s;
            }
        }
    }

    auto atlas = bptr<class anim_atlas>{InPlace, name, tex, move(anim_info)};
    return atlas;
}

auto anim_traits::make_cell(StringView name) -> Optional<Cell>
{
    return { InPlace, Cell { .atlas = {}, .name = name } };
}

} // namespace floormat::loader_detail
