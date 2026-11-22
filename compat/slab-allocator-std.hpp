#pragma once
#include "slab-allocator.inl"
#include "assert.hpp"
#include <type_traits>

namespace floormat::slab {

template<typename T, typename Resource>
class std_allocator
{
    Resource* _resource;

    static consteval bool has_slab_pool_for_objects();

public:
    using value_type = T;
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::false_type;
    using propagate_on_container_swap = std::false_type;

    explicit constexpr std_allocator(Resource* resource) noexcept;
    template<typename U> constexpr std_allocator(const std_allocator<U, Resource>& other) noexcept;

    [[nodiscard]] T* allocate(usize n);
    void deallocate(T* p, usize n) noexcept;
    std_allocator select_on_container_copy_construction() const noexcept;
    constexpr Resource* resource() const noexcept;
};

template<typename T, typename U, typename Resource>
constexpr bool operator==(const std_allocator<T, Resource>& a, const std_allocator<U, Resource>& b) noexcept;

namespace detail {

template<typename T, auto Resource> consteval auto* global_resource() noexcept
{
    if constexpr (std::is_pointer_v<decltype(Resource)>)
        return Resource;
    else
        return std::remove_cv_t<decltype(Resource)>::template resource<T>;
}

} // namespace detail

template<typename T, auto Resource>
class global_std_allocator
{
    using resource_type = std::remove_pointer_t<decltype(detail::global_resource<T, Resource>())>;

public:
    using value_type = T;
    using is_always_equal = std::true_type;
    template<typename U> struct rebind { using other = global_std_allocator<U, Resource>; };

    constexpr global_std_allocator() noexcept = default;
    template<typename U>
    constexpr global_std_allocator(const global_std_allocator<U, Resource>& other) noexcept;

    [[nodiscard]] T* allocate(usize n);
    void deallocate(T* p, usize n) noexcept;
};

template<typename T, typename U, auto Resource>
constexpr bool operator==(const global_std_allocator<T, Resource>& a,
                          const global_std_allocator<U, Resource>& b) noexcept;

template<typename T, typename Resource>
constexpr std_allocator<T, Resource>::std_allocator(Resource* resource) noexcept: _resource{resource}
{
}

template<typename T, typename Resource>
template<typename U>
constexpr std_allocator<T, Resource>::std_allocator(const std_allocator<U, Resource>& other) noexcept:
    _resource{other.resource()}
{
}

template<typename T, typename Resource>
consteval bool std_allocator<T, Resource>::has_slab_pool_for_objects()
{
    if constexpr (requires { Resource::has_slabs; })
        return Resource::has_slabs && Resource::size_class_index(sizeof(T), alignof(T)) != (u32)-1;
    else
        return false;
}

template<typename T, typename Resource>
T* std_allocator<T, Resource>::allocate(usize n)
{
    static_assert(alignof(T) <= Resource::max_alignment);
    if (n == 1)
    {
        if constexpr (has_slab_pool_for_objects())
        {
            using U = std::conditional_t<Resource::template has_own_pool<T>, T, void>;
            constexpr auto& P = Resource::template pool_constants<sizeof(T), alignof(T)>;
            auto* pool = _resource->template find_pool<sizeof(T), alignof(T), U>();
            if (!pool) [[unlikely]]
                pool = _resource->template find_or_create_pool<sizeof(T), alignof(T), U>();
            char* const p = detail::allocate_buffer_in_pool<P.num_bitmap_words_per_slab,
                                                            P.is_deallocation_lock_free()>(*pool, P.buffer_bytes);
            detail::unpoison_memory_region(p, sizeof(T));
            return static_cast<T*>(assume_aligned<P.buffer_align>(static_cast<void*>(p)));
        }
        else
            return static_cast<T*>(_resource->template allocate<sizeof(T), alignof(T), T>());
    }
    else
    {
        fm_assert(n <= (usize)-1 / sizeof(T));
        return static_cast<T*>(_resource->template allocate_array<alignof(T)>(n * sizeof(T)));
    }
}

template<typename T, typename Resource>
void std_allocator<T, Resource>::deallocate(T* p, usize n) noexcept
{
    if (n == 1)
    {
        if constexpr (has_slab_pool_for_objects())
        {
            using U = std::conditional_t<Resource::template has_own_pool<T>, T, void>;
            constexpr auto& P = Resource::template pool_constants<sizeof(T), alignof(T)>;
            constexpr bool is_lock_free = P.is_deallocation_lock_free();
            auto* const pool = _resource->template find_pool<sizeof(T), alignof(T), U>();
            fm_assert(pool);
            detail::deallocate_buffer_in_pool<P.num_bitmap_words_per_slab, is_lock_free,
                                              !is_lock_free && P.is_slab_of_last_deallocation_cached,
                                              P.is_empty_slab_given_back>(
                pool->P, pool->s, p, P.buffer_bytes, P.buffer_index_multiplier, P.slab_bytes, P.num_buffers_per_slab);
        }
        else
            _resource->template deallocate<sizeof(T), alignof(T), T>(p);
    }
    else
        _resource->template deallocate_array<alignof(T)>(p, n * sizeof(T));
}

template<typename T, typename Resource>
std_allocator<T, Resource> std_allocator<T, Resource>::select_on_container_copy_construction() const noexcept
{
    return *this;
}

template<typename T, typename Resource>
constexpr Resource* std_allocator<T, Resource>::resource() const noexcept
{
    return _resource;
}

template<typename T, typename U, typename Resource>
constexpr bool operator==(const std_allocator<T, Resource>& a, const std_allocator<U, Resource>& b) noexcept
{
    return a.resource() == b.resource();
}

template<typename T, auto Resource>
template<typename U>
constexpr global_std_allocator<T, Resource>::global_std_allocator(
    const global_std_allocator<U, Resource>&) noexcept
{
}

template<typename T, auto Resource>
T* global_std_allocator<T, Resource>::allocate(usize n)
{
    return std_allocator<T, resource_type>{detail::global_resource<T, Resource>()}.allocate(n);
}

template<typename T, auto Resource>
void global_std_allocator<T, Resource>::deallocate(T* p, usize n) noexcept
{
    std_allocator<T, resource_type>{detail::global_resource<T, Resource>()}.deallocate(p, n);
}

template<typename T, typename U, auto Resource>
constexpr bool operator==(const global_std_allocator<T, Resource>&,
                          const global_std_allocator<U, Resource>&) noexcept
{
    return true;
}

} // namespace floormat::slab
