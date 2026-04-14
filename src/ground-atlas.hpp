#pragma once
#include "compat/borrowed-ptr.hpp"
#include "src/pass-mode.hpp"
#include "src/quads.hpp"
#include "src/ground-def.hpp"
#include "loader/ground-cell.hpp"
#include "loader/sprite-atlas.hpp"
#include <array>
#include <cr/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/String.h>
#include <cr/Pointer.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

class ground_atlas;

class ground_atlas final : public bptr_base
{
    using quad = Quads::quad;
    using texcoords = Quads::texcoords;

    static String make_path(StringView name);

    ground_def _def;
    String _path;
    Array<sprite> _frame_sprites;

public:
    ground_atlas(ground_def info, const ImageView2D& img);
    texcoords texcoords_for_id(size_t id) const;
    size_t num_tiles() const;
    Vector2ub num_tiles2() const { return _def.size; }
    ArrayView<const sprite> raw_sprite_array() const;
    StringView name() const { return _def.name; }
    enum pass_mode pass_mode() const;
};

} // namespace floormat
