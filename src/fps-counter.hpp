#pragma once
#include "nanosecond.hpp"

namespace floormat {

class FPS_Counter
{
    double frame_time = 0;  // seconds, smoothed
    double change_rate = 0; // relative change of frame_time per second, smoothed
    Ns total_time{};
    Ns settle_time{};
    bool value_ok = false;

public:
    explicit FPS_Counter(Ns settle_time = Ns{}) noexcept;

    CORRADE_NEVER_INLINE void reset();
    CORRADE_NEVER_INLINE float get() const;
    CORRADE_NEVER_INLINE float update(Ns dt);

    CORRADE_NEVER_INLINE void set_settle_time(Ns time);
    CORRADE_NEVER_INLINE Ns get_settle_time() const;
};

} // namespace floormat
