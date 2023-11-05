#include "wall-atlas.hpp"
#include "magnum-vector2i.hpp"
#include "magnum-vector.hpp"
#include "compat/exception.hpp"
#include <utility>
#include <string_view>
#include <Corrade/Containers/StringStl.h>
#include <nlohmann/json.hpp>

// todo add test on dummy files that generates 100% coverage on the j.contains() blocks!

namespace floormat {

namespace {

using namespace std::string_literals;
constexpr StringView rotation_names[] = { "n"_s, "e"_s, "s"_s, "w"_s, };

size_t rotation_from_name(StringView s)
{
    for (auto i = 0uz; auto n : rotation_names)
    {
        if (n == s)
            return i;
        i++;
    }
    fm_throw("bad rotation name '{}'"_cf, fmt::string_view{s.data(), s.size()});
}

StringView rotation_to_name(size_t i)
{
    fm_soft_assert(i < std::size(rotation_names));
    return rotation_names[i];
}

void read_frameset_metadata(const nlohmann::json& j, wall_frames& val, size_t& rot)
{
    val = {};
    rot = rotation_from_name(std::string{j["name"s]});

    if (j.contains("pixel-size"s))
        val.pixel_size = j["pixel-size"s];
    if (j.contains("tint"s))
    {
        std::tie(val.tint_mult, val.tint_add) = std::pair<Vector4, Vector3>{j["tint"s]};
        fm_soft_assert(val.tint_mult >= Color4{0});
    }
    if (j.contains("from-rotation"s))
        val.from_rotation = (uint8_t)rotation_from_name(std::string{j["from-rotation"s]});
    if (j.contains("mirrored"s))
        val.mirrored = j["mirrored"s];
    if (j.contains("use-default-tint"s))
        val.use_default_tint = j["use-default-tint"s];
}

void write_frameset_metadata(nlohmann::json& j, const wall_atlas& a, const wall_frames& val, size_t rot)
{
    constexpr wall_frames default_value;

    fm_soft_assert(val.count != (uint32_t)-1);
    fm_soft_assert(val.index == (uint32_t)-1 || val.index < a.frame_array().size());
    fm_soft_assert((val.index == (uint32_t)-1) == (val.count == 0));

    j["name"s] = rotation_to_name(rot);
    j["pixel-size"s] = val.pixel_size;
    if (val.tint_mult != default_value.tint_mult || val.tint_add != default_value.tint_add)
    {
        auto tint = std::pair<Vector4, Vector3>{{val.tint_mult}, {val.tint_add}};
        j["tint"s] = tint;
    }
    if (val.from_rotation != default_value.from_rotation)
    {
        fm_soft_assert(val.from_rotation != (uint8_t)-1 && val.from_rotation < 4);
        j["from-rotation"s] = val.from_rotation;
    }
    if (val.mirrored != default_value.mirrored)
        j["mirrored"s] = val.mirrored;
    if (val.use_default_tint)
        if (val.tint_mult != default_value.tint_mult || val.tint_add != default_value.tint_add)
            j["use-default-tint"s] = true;
}

void read_framesets(const nlohmann::json& jf, wall_atlas_def& val)
{
    fm_soft_assert(jf.is_object());
    fm_soft_assert(val.framesets == nullptr && val.frameset_count == 0);
    uint8_t count = 0;

    static_assert(std::size(rotation_names) == 4);
    for (auto i = 0uz; i < 4; i++)
    {
        const auto& r = rotation_names[i];
        auto key = std::string_view{r.data(), r.size()};
        if (jf.contains(key))
        {
            fm_soft_assert(jf[key].is_object());
            auto& index = val.frameset_indexes[i];
            fm_soft_assert(index == (uint8_t)-1);
            index = count++;
        }
    }
    fm_soft_assert(count > 0);
    fm_soft_assert(count == jf.size());

    val.framesets = std::make_unique<wall_frame_set[]>(count);
    val.frameset_count = count;
}

} // namespace

void Serialize::wall_test::read_atlas_header(const nlohmann::json& j, wall_atlas_def& val)
{
    val = {};
    val.info = { std::string(j["name"s]), j["depth"], };
    val.frame_count = (uint32_t)j["frames"s].size();
    read_framesets(j["framesets"s], val);
}

} // namespace floormat

namespace nlohmann {

using namespace floormat;

void adl_serializer<std::shared_ptr<wall_atlas>>::to_json(json& j, const std::shared_ptr<const wall_atlas>& x)
{

}

void adl_serializer<std::shared_ptr<wall_atlas>>::from_json(const json& j, std::shared_ptr<wall_atlas>& x)
{

}

} // namespace nlohmann
