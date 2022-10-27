#pragma once

#include "compat/integer-types.hpp"
#include "compat/defs.hpp"
#include "compat/assert.hpp"

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <concepts>
#include <type_traits>
#include <limits>

#include <tuple>
#include <array>
#include <optional>
#include <vector>
#include <memory>
#include <utility>
#include <algorithm>
#include <filesystem>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/StringStlView.h>
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
