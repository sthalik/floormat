#pragma once
#include "nanosecond.hpp"

namespace floormat {

class FPS_Counter
{
    struct Impl;

    Impl* p ;

public:
    explicit FPS_Counter(Ns settle_time = Ns{}) noexcept;
    ~FPS_Counter() noexcept;

    CORRADE_NEVER_INLINE void reset();
    CORRADE_NEVER_INLINE float get() const;
    CORRADE_NEVER_INLINE float update(Ns dt);

    CORRADE_NEVER_INLINE void set_settle_time(Ns time);
    CORRADE_NEVER_INLINE Ns get_settle_time() const;

    CORRADE_NEVER_INLINE FPS_Counter(const FPS_Counter& other) noexcept;
    CORRADE_NEVER_INLINE FPS_Counter& operator=(const FPS_Counter& other) noexcept;

    FPS_Counter(FPS_Counter&&) noexcept = delete;
    FPS_Counter& operator=(FPS_Counter&&) noexcept = delete;
};

} // namespace floormat
