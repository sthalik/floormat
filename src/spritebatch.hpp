#pragma once
#include "compat/defs.hpp"
#include "compat/safe-ptr.hpp"
#include "rotation.hpp"
#include "quads.hpp"

namespace floormat {

struct point;
struct tile_shader;
struct object;
class anim_atlas;
class chunk;
struct SpriteList;

class SpriteBatch
{
    struct Impl;

    void ensure_allocated(u32 count);

    safe_ptr<Impl> impl;
    float _depth_start = -1;

public:
    void begin_chunk(u32 max_quads);
    template<bool do_sort> void end_chunk();
    void clear();
    void draw(tile_shader& shader, bool do_sort = true);

    // Needs no GL. Together these let a test observe the merge; without merged_order() a
    // dropped or duplicated quad is unobservable from outside.
    void sort_vertex_buffer(bool do_sort);
    ArrayView<const u32> merged_order() const;
    void unsort_vertex_buffer();

    void emit(const Quads::vertexes& vertexes, float depth);

    void emit(SpriteList& list);
    void sort_by_depth(SpriteList& list);

    void emit_quick(tile_shader& shader, const anim_atlas& atlas, rotation r, usize frame, const Vector3& center, const Quads::depths& depth);

    float depth_start() const;
    void set_depth_start(float value);

    explicit SpriteBatch();
    ~SpriteBatch() noexcept;
    fm_DISABLE_MOVE_COPY(SpriteBatch);
};

} // namespace floormat
