#pragma once

#include "compat/prelude.hpp"
#include "compat/integer-types.hpp"
#include "compat/defs.hpp"
#include "compat/assert.hpp"

#if __has_include(<fmt/core.h>)
#include "compat/exception.hpp"
#endif

#ifdef __GNUG__
#pragma GCC system_header
#endif

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>
#include <algorithm>

#include <tuple>
#include <bitset>
#include <array>
#include <vector>
#include <unordered_map>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/Pointer.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringView.h>

#include <Corrade/Containers/PairStl.h>
#include <Corrade/Containers/TripleStl.h>

#include <Corrade/Utility/Debug.h>
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
