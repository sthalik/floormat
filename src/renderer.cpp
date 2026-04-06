#include "renderer.hpp"
#include "compat/spinlock.hpp"

namespace floormat::Render {

namespace {

Spinlock spl;
bool clipcontrol_clipdepth_zero_one_status = false;

} // namespace

void detail::set_clipcontrol_clipdepth_zero_one_status(bool value)
{
    Locker l{spl};
    clipcontrol_clipdepth_zero_one_status = value;
}

Status get_status()
{
    Locker l{spl};
    return {
        .is_clipdepth01_enabled = clipcontrol_clipdepth_zero_one_status,
    };
}

} // namespace floormat::Render
