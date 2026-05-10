#pragma once
#include "compat/defs.hpp"
#include "tile-defs.hpp"
#include <array>
#include <cr/Array.h>

namespace floormat {
class world;
class chunk;
struct chunk_coords_;
} // namespace floormat

namespace floormat::detail {

class memo_chunk
{
    static constexpr int z_count = chunk_z_max - chunk_z_min + 1;

    Array<chunk*> _data; // todo add superpages
    int _x_min = 0, _x_max = 0;
    int _y_min = 0, _y_max = 0;

    size_t index_of(chunk_coords_ ch) const;
    void grow_for(world& w, chunk_coords_ trigger_ch);

public:
    explicit memo_chunk();
    ~memo_chunk() noexcept;
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(memo_chunk);

    chunk* chunk_at(world& w, chunk_coords_ ch);
    const chunk* chunk_at(const world& w, chunk_coords_ ch) const;
    std::array<chunk*, 8> neighbors(world& w, chunk_coords_ ch0);
    std::array<const chunk*, 8> neighbors(const world& w, chunk_coords_ ch0) const;
    void update_slot(world& w, chunk_coords_ ch, chunk* p);
    void prepare_next_frame(world& w);
#ifndef FM_NO_DEBUG2
    void check_in_sync(world& w) const;
#endif
};

} // namespace floormat::detail
