#pragma once

namespace floormat {

enum class object_type : unsigned char {
    none, critter, scenery, light, COUNT,
};
template<typename T> struct object_type_;

} // namespace floormat
