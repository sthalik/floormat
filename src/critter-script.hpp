#pragma once

namespace floormat {

struct critter;
struct Ns;

struct base_script
{
    virtual ~base_script() noexcept;
    virtual void delete_self() = 0;
    base_script() noexcept;
};

template<typename T>
class script_wrapper final
{
    static_assert(std::is_base_of_v<base_script, T>);
    T* ptr;

public:
    explicit script_wrapper(T* ptr);
    ~script_wrapper() noexcept;

    const T& operator*() const noexcept;
    T& operator*() noexcept;
    const T* operator->() const noexcept;
    T* operator->() noexcept;
};

struct critter_script : base_script
{
    critter_script(critter& c);
    virtual void update(critter& c, size_t i, const Ns& dt) = 0;
    // todo can_activate, activate
};

extern template class script_wrapper<critter_script>;

} // namespace floormat
