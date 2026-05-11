#pragma once
#include "compat/defs.hpp"
#include <array>
#include <cr/Array.h>
#include <cr/Pointer.h>

namespace floormat {
class world;
class chunk;
struct chunk_coords_;
} // namespace floormat

namespace floormat::detail {

struct chunk_table_leaf;

class chunk_table
{
public:
    explicit chunk_table();
    ~chunk_table() noexcept;
    fm_DISABLE_COPY(chunk_table);

    chunk*       chunk_at(chunk_coords_ ch) noexcept;
    const chunk* chunk_at(chunk_coords_ ch) const noexcept;

    std::array<chunk*, 8>       neighbors(chunk_coords_ ch0) noexcept;
    std::array<const chunk*, 8> neighbors(chunk_coords_ ch0) const noexcept;

    void update_slot(chunk_coords_ ch, chunk* p) noexcept;

#ifndef FM_NO_DEBUG2
    void check_in_sync(const world& w) const;
#endif

private:
    Array<Pointer<chunk_table_leaf>> _outer;
};

} // namespace floormat::detail
