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
    PoolRegistry();
    explicit PoolRegistry(uint32_t div_size);
    ~PoolRegistry() noexcept;
    fm_DECLARE_DELETED_COPY_MOVE_ASSIGNMENTS(PoolRegistry);

    Pool& pool_for(uint32_t bbox_size);

    void maybe_mark_stale_all(uint64_t frame_no);
    void build_if_stale_all();

    uint32_t live_pool_count() const;

    static constexpr uint32_t direct_count = 127;
    static constexpr uint32_t direct_max_bbox = 254;
    static constexpr uint32_t min_bbox_size = 4;        // mirrors Params::validate()

private:
    uint64_t last_frame_no_ = (uint64_t)-1;
    std::array<Pointer<Pool>, direct_count> direct_;
    Array<Pointer<Pool>> fallback_;
    uint32_t div_size_;
};

} // namespace floormat::Grid::Pass
