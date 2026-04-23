#include "impl.hpp"
#include "compat/exception.hpp"
#include "compat/hash-table-load-factor.hpp"
#include "src/anim-atlas.hpp"
#include "src/anim.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/corrade-string.hpp"
#include "loader/vobj-cell.hpp"
#include <cr/ArrayViewStl.h>
#include <cr/StridedArrayView.h>
#include <cr/Path.h>
#include <mg/ImageData.h>
#include <mg/ImageView.h>

namespace floormat::loader_detail {
struct vobj {
    String name, descr, image_filename;
};
} // namespace floormat::loader_detail

namespace nlohmann {

using floormat::loader_detail::vobj;

template<>
struct adl_serializer<vobj> {
    static void to_json(json& j, const vobj& val);
    static void from_json(const json& j, vobj& val);
};

void adl_serializer<vobj>::to_json(json& j, const vobj& val)
{
    j["name"] = val.name;
    if (val.descr)
        j["description"] = val.descr;
    j["image"] = val.image_filename;
}

void adl_serializer<vobj>::from_json(const json& j, vobj& val)
{
    val = {};
    val.name = j["name"];
    if (j.contains("description"))
        val.descr = j["description"];
    else
        val.descr = val.name;
    val.image_filename = j["image"];
}

} // namespace nlohmann

namespace floormat::loader_detail {

bptr<class anim_atlas> loader_impl::make_vobj_anim_atlas(StringView name, StringView image_filename)
{
    auto tex = texture(VOBJ_PATH, image_filename);
    anim_def def;
    def.object_name = name;
    const auto size = tex.pixels().size();
    const auto width = (unsigned)size[1], height = (unsigned)size[0];
    def.pixel_size = { width, height };
    def.nframes = 1;
    def.fps = 0;
    {
        auto group = anim_group {
            .name = "n"_s,
            .frames = { InPlaceInit, {
                anim_frame {
                    .ground = Vector2i(def.pixel_size/2),
                    .size = def.pixel_size,
                }},
            },
        };
        def.groups = Array<anim_group>{1};
        def.groups[0] = move(group);
    }

    const auto format = tex.format();
    const auto px_size = (size_t)tex.pixels().size()[2];  // 3 for RGB, 4 for RGBA
    const auto row_stride = (size_t)width * px_size;
    const auto* src_base = (const char*)tex.pixels().data();
    PixelStorage storage;
    storage.setRowLength((Int)width);
    storage.setAlignment(1);
    const ImageView2D view{storage, format,
                           {(Int)width, (Int)height},
                           ArrayView<const void>{src_base, (size_t)height * row_stride}};
    auto sp = atlas().add(view);
    def.groups[0].sprites = Array<const SpriteAtlas::Sprite*>{ValueInit, 1};
    def.groups[0].sprites[0] = sp.raw();

    return bptr<class anim_atlas>(InPlace, name, tex, move(def));
}

void loader_impl::get_vobj_list()
{
    fm_assert(vobjs.empty());

    vobjs.clear();
    vobj_atlas_map.clear();
    auto vec = json_helper::from_json<std::vector<struct vobj>>(Path::join(VOBJ_PATH, "vobj.json"));
    vec.shrink_to_fit();

    vobjs.reserve(vec.size());
    Hash::set_separate_chaining_load_factor(vobj_atlas_map, vec.size());

    for (const auto& [name, descr, img_name] : vec)
    {
        auto atlas = make_vobj_anim_atlas(name, img_name);
        auto info = vobj_cell{name, descr, atlas};
        vobjs.push_back(move(info));
        const auto& x = vobjs.back();
        vobj_atlas_map[x.atlas->name()] = &x;
    }

    fm_assert(!vobjs.empty());
}

ArrayView<const vobj_cell> loader_impl::vobj_list()
{
    if (vobjs.empty())
        get_vobj_list();
    fm_assert(!vobjs.empty());
    return vobjs;
}

const struct vobj_cell& loader_impl::vobj(StringView name)
{
    if (vobjs.empty())
        get_vobj_list();
    auto it = vobj_atlas_map.find(name);
    if (it == vobj_atlas_map.end())
        fm_throw("no such vobj '{}'"_cf, name);
    return *it->second;
}

} // namespace floormat::loader_detail
