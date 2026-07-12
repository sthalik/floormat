#pragma once
#include "src/quads.hpp"
#include <mg/Mesh.h>

namespace floormat::Quads {

namespace detail {
template<typename T> struct mesh_index_type;
template<> struct mesh_index_type<UnsignedByte>  { static constexpr auto value = GL::MeshIndexType::UnsignedByte;  };
template<> struct mesh_index_type<UnsignedShort> { static constexpr auto value = GL::MeshIndexType::UnsignedShort; };
template<> struct mesh_index_type<UnsignedInt>   { static constexpr auto value = GL::MeshIndexType::UnsignedInt;   };
} // namespace detail

template<typename T> constexpr inline GL::MeshIndexType index_gl_type_v = detail::mesh_index_type<T>::value;
constexpr inline GL::MeshIndexType index_gl_type = index_gl_type_v<index_type>;

} // namespace floormat::Quads
