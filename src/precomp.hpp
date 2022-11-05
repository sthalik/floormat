#pragma once

#include "compat/integer-types.hpp"
#include "compat/defs.hpp"
#include "compat/assert.hpp"
#include "compat/alloca.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cmath>

#include <concepts>
#include <limits>
#include <memory>
#include <type_traits>

#include <algorithm>
#include <utility>
#include <iterator>
#include <filesystem>

#include <tuple>
#include <array>
#include <optional>
#include <vector>
#include <unordered_map>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayViewStl.h> // TODO maybe remove stl
#include <Corrade/Containers/StringStlView.h> // TODO remove stl
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/Pointer.h>
#include <Corrade/Utility/DebugStl.h>

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

#include <nlohmann/json.hpp>
