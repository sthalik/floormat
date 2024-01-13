#include "impl.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/corrade-string.hpp"
#include "src/anim-atlas.hpp"
#include "src/anim.hpp"
#include "compat/exception.hpp"
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Trade/ImageData.h>

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
    val.name = j["name"];
    if (j.contains("description"))
        val.descr = j["description"];
    else
        val.descr = val.name;
    val.image_filename = j["image"];
}

} // namespace nlohmann

namespace floormat::loader_detail {

std::shared_ptr<class anim_atlas> loader_impl::make_vobj_anim_atlas(StringView name, StringView image_filename)
{
    auto tex = texture(VOBJ_PATH, image_filename);
    anim_def def;
    def.object_name = name;
    const auto size = tex.pixels().size();
    const auto width = (unsigned)size[1], height = (unsigned)size[0];
    def.pixel_size = { width, height };
    def.nframes = 1;
    def.fps = 0;
    def.groups = {{
        .name = "n"_s,
        .frames = {{
            .ground = Vector2i(def.pixel_size/2),
            .size = def.pixel_size
        }}
    }};
    auto atlas = std::make_shared<class anim_atlas>(name, tex, std::move(def));
    return atlas;
}

void loader_impl::get_vobj_list()
{
    fm_assert(vobjs.empty());

    vobjs.clear();
    vobj_atlas_map.clear();
    auto vec = json_helper::from_json<std::vector<struct vobj>>(Path::join(VOBJ_PATH, "vobj.json"));
    vec.shrink_to_fit();

    vobjs.reserve(vec.size());
    vobj_atlas_map.reserve(2*vec.size());

    for (const auto& [name, descr, img_name] : vec)
    {
        auto atlas = make_vobj_anim_atlas(name, img_name);
        auto info = vobj_info{name, descr, atlas};
        vobjs.push_back(std::move(info));
        const auto& x = vobjs.back();
        vobj_atlas_map[x.atlas->name()] = &x;
    }

    fm_assert(!vobjs.empty());
}

ArrayView<const vobj_info> loader_impl::vobj_list()
{
    if (vobjs.empty())
        get_vobj_list();
    fm_assert(!vobjs.empty());
    return vobjs;
}

const struct vobj_info& loader_impl::vobj(StringView name)
{
    if (vobjs.empty())
        get_vobj_list();
    auto it = vobj_atlas_map.find(name);
    if (it == vobj_atlas_map.end())
        fm_throw("no such vobj '{}'"_cf, name);
    return *it->second;
}

} // namespace floormat::loader_detail
