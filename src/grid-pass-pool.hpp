#pragma once

#include "compat/defs.hpp"
#include "grid-pass.hpp"
#include "grid-cover.hpp"
#include <array>
#include <bit>
#include <cr/Pointer.h>
#include <cr/Array.h>

namespace floormat::Grid {

class PoolRegistry final
{
public:
    explicit PoolRegistry(uint32_t div_size);
    ~PoolRegistry() noexcept;
    fm_DISABLE_MOVE_COPY(PoolRegistry);

    Pass::Pool& pass_pool(const Pass::Params& p);
    Pass::Pool& pool_for(uint32_t bbox_size);
    Cover::Pool& cover_pool(const Cover::Params& p);

    static constexpr uint32_t direct_count = 127;
    static constexpr uint32_t direct_max_bbox = 254;

private:
    // cover pools are indexed by countr_zero(div_size)
    static_assert(std::has_single_bit(chunk_size_xy));

    std::array<Pointer<Pass::Pool>, direct_count> direct_;
    Array<Pointer<Pass::Pool>> fallback_;
    // after the pass pools, so destroyed first: each borrows one of them
    std::array<Pointer<Cover::Pool>, std::bit_width(chunk_size_xy)> cover_;
    uint32_t div_size_;
};

} // namespace floormat::Grid
