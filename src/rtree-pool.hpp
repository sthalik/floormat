#pragma once
#include "compat/move.hpp"

namespace floormat::detail {

template<typename T> struct rtree_pool final
{
    rtree_pool() noexcept;
    rtree_pool(const rtree_pool&) = delete;
    rtree_pool& operator=(const rtree_pool&) = delete;
    ~rtree_pool() noexcept;
    size_t alive_count() const;

    template<typename... Xs>
    T* construct(Xs&&... args)
    noexcept(std::is_nothrow_constructible_v<T, decltype(forward<Xs>(args))...>);

    void free(T* pool) noexcept(std::is_nothrow_destructible_v<T>);

    union node_u
    {
        T data;
        node_u* next;

        inline node_u() noexcept {}
        inline ~node_u() noexcept {}
    };

private:
    node_u* free_list = nullptr;
    size_t live_count = 0;
};

} // namespace floormat::detail
