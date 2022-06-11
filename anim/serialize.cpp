#include "serialize.hpp"
#include "../json.hpp"

#include <algorithm>
#include <utility>
#include <fstream>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/DebugStl.h>

using Corrade::Utility::Error;

namespace nlohmann {

template<>
struct adl_serializer<Magnum::Vector2i> final {
    static void to_json(json& j, const Magnum::Vector2i& x);
    static void from_json(const json& j, Magnum::Vector2i& x);
};

void adl_serializer<Magnum::Vector2i>::to_json(json& j, const Magnum::Vector2i& x)
{
    j["x"] = x[0];
    j["y"] = x[1];
}

void adl_serializer<Magnum::Vector2i>::from_json(const json& j, Magnum::Vector2i& x)
{
    j.at("x").get_to(x[0]);
    j.at("y").get_to(x[1]);
}

} // namespace nlohmann

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_frame, ground, offset, size);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_group, name, frames, ground);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim, name, nframes, actionframe, fps, groups);

std::tuple<anim, bool> anim::from_json(const std::filesystem::path& pathname)
{
    using namespace nlohmann;
    std::ifstream s;
    s.exceptions(s.exceptions() | std::ios::failbit | std::ios::badbit);
    try {
        s.open(pathname, std::ios_base::in);
    } catch (const std::ios::failure& e) {
        Error{Error::Flag::NoSpace} << "failed to open '" << pathname << "':" << e.what();
        return { {}, false };
    }
    anim ret;
    try {
        json j;
        s >> j;
        using nlohmann::from_json;
        from_json(j, ret);
    } catch (const std::exception& e) {
        Error{Error::Flag::NoSpace} << "failed to parse '" << pathname << "':" << e.what();
        return { {}, false };
    }
    return { std::move(ret), true };
}

bool anim::to_json(const std::filesystem::path& pathname)
{
    nlohmann::json j = *this;

    std::ofstream s;
    s.exceptions(s.exceptions() | std::ios::failbit | std::ios::badbit);
    try {
        s.open(pathname, std::ios_base::out | std::ios_base::trunc);
    } catch (const std::ios::failure& e) {
        Error{Error::Flag::NoSpace} << "failed to open '" << pathname << "' for writing: " << e.what();
        return false;
    }
    try {
        s << j.dump(4);
        s.flush();
    } catch (const std::exception& e) {
        Error{Error::Flag::NoSpace} << "failed writing '" << pathname << "' :" << e.what();
        return false;
    }

    return true;
}
