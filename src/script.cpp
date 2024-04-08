#include "script.inl"
#include <cr/StringView.h>

namespace floormat {

namespace {

constexpr StringView names[(size_t)script_lifecycle::COUNT] =
{
    "no-init"_s, "initializing"_s, "created"_s, "destroying"_s, "torn_down"_s,
};

} // namespace

StringView base_script::state_name(script_lifecycle x)
{
    if (x >= script_lifecycle::COUNT)
        fm_abort("invalid script_lifecycle value '%d'", (int)x);
    else
        return names[(uint32_t)x];
}

base_script::~base_script() noexcept = default;
base_script::base_script() noexcept = default;

} // namespace floormat
