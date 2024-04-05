#include "critter-script.inl"

namespace floormat {

template class script_wrapper<critter_script>;

base_script::~base_script() noexcept = default;
base_script::base_script() noexcept = default;

} // namespace floormat
