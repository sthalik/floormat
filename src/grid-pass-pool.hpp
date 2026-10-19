#pragma once

#include "compat/defs.hpp"
#include "grid-pass.hpp"
#include <array>
#include <cr/Pointer.h>
#include <cr/Array.h>

namespace floormat::Grid::Pass {

class PoolRegistry final
{
public:
    explicit PoolRegistry(uint32_t div_size);
    ~PoolRegistry() noexcept;
    fm_DISABLE_MOVE_COPY(PoolRegistry);

    Pool& pool_for(uint32_t bbox_size);

    void maybe_mark_stale_all(const world& w);

    static constexpr uint32_t direct_count = 127;
    static constexpr uint32_t direct_max_bbox = 254;

private:
    std::array<Pointer<Pool>, direct_count> direct_;
    Array<Pointer<Pool>> fallback_;
    uint32_t div_size_;
};

} // namespace floormat::Grid::Pass
