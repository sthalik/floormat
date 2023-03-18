#pragma once

#include "compat/prelude.hpp"
#include "compat/integer-types.hpp"
#include "compat/defs.hpp"
#include "compat/assert.hpp"

#ifdef __GNUG__
#pragma GCC system_header
#endif

#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>

#include <concepts>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>

#include <algorithm>
#include <utility>

#include <tuple>
#include <bitset>
#include <array>
#include <vector>
#include <unordered_map>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/EnumSet.h>
#include <Corrade/Containers/Pointer.h>
#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringStlHash.h>

#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Resource.h>
#include <Corrade/Utility/Path.h>

#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Math/Color.h>
#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/Trade/ImageData.h>
