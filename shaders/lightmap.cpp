#include "lightmap.hpp"
#include "src/local-coords.hpp"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

namespace floormat {

lightmap_shader::~lightmap_shader() = default;
bool lightmap_shader::light_s::operator==(const light_s&) const noexcept = default;

static constexpr Vector2 output_size = TILE_MAX_DIM * TILE_SIZE2 * 3;

lightmap_shader::lightmap_shader()
{

}

} // namespace floormat
