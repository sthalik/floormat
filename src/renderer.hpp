#pragma once

namespace floormat::Render::detail {

void set_clipcontrol_clipdepth_zero_one_status(bool value);

}  // namespace floormat::Render::detail

namespace floormat::Render {

struct Status
{
    bool is_clipcontrol_clipdepth_zero_one_enabled = false;
};

Status get_status();

}  // namespace floormat::Render
