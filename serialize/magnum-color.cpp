#include "magnum-color.hpp"
#include <array>
#include <Magnum/Math/Color.h>
#include <nlohmann/json.hpp>

namespace floormat {

namespace {

using c3_proxy = std::array<float, 3>;
using c4_proxy = std::array<float, 4>;
using c3ub_proxy = std::array<uint8_t, 3>;
using c4ub_proxy = std::array<uint8_t, 4>;

} // namespace

} // namespace floormat

namespace nlohmann {

using namespace floormat;

void adl_serializer<Color3>::to_json(json& val, const Color3& p)        { val    = c3_proxy  { p[0], p[1], p[2] }; }
void adl_serializer<Color3ub>::to_json(json& val, const Color3ub& p)    { val    = c3ub_proxy{ p[0], p[1], p[2] }; }
void adl_serializer<Color4>::to_json(json& val, const Color4& p)        { val    = c4_proxy  { p[0], p[1], p[2], p[3] }; }
void adl_serializer<Color4ub>::to_json(json& val, const Color4ub& p)    { val    = c4ub_proxy{ p[0], p[1], p[2], p[3] }; }

void adl_serializer<Color3>::from_json(const json& j, Color3& val)      { auto p = c3_proxy{j};   val = { p[0], p[1], p[2] }; }
void adl_serializer<Color3ub>::from_json(const json& j, Color3ub& val)  { auto p = c3ub_proxy{j}; val = { p[0], p[1], p[2] }; }
void adl_serializer<Color4>::from_json(const json& j, Color4& val)      { auto p = c4_proxy{j};   val = { p[0], p[1], p[2], p[3] }; }
void adl_serializer<Color4ub>::from_json(const json& j, Color4ub& val)  { auto p = c4ub_proxy{j}; val = { p[0], p[1], p[2], p[3] }; }

} // namespace nlohmann
