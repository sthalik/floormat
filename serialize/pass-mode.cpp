#include "pass-mode.hpp"
#include "compat/exception.hpp"
#include "serialize/corrade-string.hpp"
#include <nlohmann/json.hpp>
#include <Corrade/Containers/StringStlView.h>

namespace nlohmann {

using namespace floormat;

static constexpr struct {
    pass_mode mode;
    StringView str;
} table[] = {
    { pass_mode::shoot_through, "shoot-through"_s },
    { pass_mode::pass, "pass"_s },
    { pass_mode::blocked, "blocked"_s },
    { pass_mode::see_through, "see-through"_s },
};

void adl_serializer<pass_mode>::to_json(json& j, pass_mode val)
{
    for (const auto [mode, str] : table)
        if (mode == val)
        {
            j = str;
            return;
        }
    fm_throw("invalid pass mode '{}'"_cf, size_t(val));
}

void adl_serializer<pass_mode>::from_json(const json& j, pass_mode& val)
{
    StringView value = j;
    for (const auto [mode, str] : table)
        if (str == value)
        {
            val = mode;
            return;
        }
    fm_throw("invalid pass mode '{}'"_cf, value);
}

} // namespace nlohmann
