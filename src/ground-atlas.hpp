#pragma once
#include "compat/borrowed-ptr.hpp"
#include "src/pass-mode.hpp"
#include "src/quads.hpp"
#include "src/ground-def.hpp"
#include "loader/ground-cell.hpp"
#include <array>
#include <cr/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/String.h>
#include <cr/Pointer.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/GL/Texture.h>

namespace floormat {

class ground_atlas;

class ground_atlas final : public bptr_base
{
    using quad = Quads::quad;
    using texcoords = std::array<Vector2, 4>;

    static Array<texcoords> make_texcoords_array(Vector2ui pixel_size, Vector2ub tile_count);
    static texcoords make_texcoords(Vector2ui pixel_size, Vector2ub tile_count, size_t i);
    static String make_path(StringView name);

    ground_def _def;
    String _path;
    Array<texcoords> _texcoords;
    GL::Texture2D _tex;
    Vector2ui _pixel_size;

public:
    ground_atlas(ground_def info, const ImageView2D& img);
    texcoords texcoords_for_id(size_t id) const;
    [[maybe_unused]] Vector2ui pixel_size() const { return _pixel_size; }
    size_t num_tiles() const;
    Vector2ub num_tiles2() const { return _def.size; }
    GL::Texture2D& texture() { return _tex; }
    StringView name() const { return _def.name; }
    enum pass_mode pass_mode() const;
};

} // namespace floormat
