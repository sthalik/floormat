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

void base_script::_assert_state(script_lifecycle old_state, script_lifecycle s, const char* file, int line)
{
    if (old_state != s) [[unlikely]]
        fm_emit_abort(file, line,
                      "invalid state transition from '%s' to '%s'",
                      state_name(old_state).data(),
                      state_name(s).data());
}

base_script::~base_script() noexcept = default;

} // namespace floormat
