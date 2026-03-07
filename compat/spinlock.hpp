#pragma once
#include "compat/defs.hpp"
#include <concepts>

namespace floormat {

class Spinlock final
{
#ifdef _MSC_VER
    long state = 0;
#else
    int32_t state = 0;
#endif

public:
    void lock() noexcept;
    [[nodiscard]] bool try_lock() noexcept;
    void unlock() noexcept;
};

template<typename T>
concept LockC = requires (T& L)
{
    { L.lock()     } -> std::same_as<void>;
    { L.unlock()   } -> std::same_as<void>;
    { L.try_lock() } -> std::convertible_to<bool>;
};

template<LockC T>
class Locker
{
    T& L;
    bool locked = false;

public:
    fm_DECLARE_DELETED_COPY_MOVE_ASSIGNMENTS(Locker);

    Locker(T& lock) noexcept;
    ~Locker() noexcept;
    void lock();
    void unlock();
};

} // namespace floormat
