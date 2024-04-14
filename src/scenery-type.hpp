#pragma once

namespace floormat {

enum class scenery_type : unsigned char {
    none, generic, door, COUNT,
};

template<typename T> struct scenery_type_;

} // namespace floormat
