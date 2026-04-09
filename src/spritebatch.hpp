#pragma once
#include "compat/defs.hpp"
#include "compat/safe-ptr.hpp"
#include "src/rotation.hpp"
#include <array>

namespace Magnum::GL {
template<UnsignedInt dimensions> class Texture;
using Texture2D = Texture<2>;
} // namespace Magnum::GL

namespace floormat::Quads {
struct vertex;
using vertexes = std::array<vertex, 4>;
using indexes = std::array<UnsignedShort, 6>;
using texcoords = std::array<Vector2, 4>;
using depths = std::array<float, 4>;
} // namespace floormat::Quads

namespace floormat {

struct point;
struct tile_shader;
struct object;
struct clickable;
class anim_atlas;
//struct Mesh;
class chunk;
struct SpriteList;

class SpriteBatch // rename to SpriteBatch
{
    struct Impl;

    void ensure_allocated(uint32_t count);
    void sort_vertex_buffer();

    safe_ptr<Impl> impl;

public:
    void begin_chunk();
    void end_chunk(bool do_sort);
    void clear();
    void draw(tile_shader& shader);

    static void add_clickable(object* obj, const tile_shader& shader, Vector2i win_size, Array<clickable>& array);
    void emit(GL::Texture2D& tex, const Quads::vertexes& vertexes, float depth);
    void emit(SpriteList& list, bool render_vobjs);

    void emit_quick(tile_shader& shader, anim_atlas& atlas, rotation r, size_t frame, const Vector3& center, const Quads::depths& depth);

    explicit SpriteBatch();
    ~SpriteBatch() noexcept;
    fm_DECLARE_DELETED_COPY_MOVE_ASSIGNMENTS(SpriteBatch);
};

} // namespace floormat
