#pragma once

namespace floormat::detail {

template<typename T> struct rtree_pool final
{
    rtree_pool() noexcept;
    rtree_pool(const rtree_pool&) = delete;
    rtree_pool& operator=(const rtree_pool&) = delete;
    ~rtree_pool() noexcept;
    T* construct();
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
};

} // namespace floormat::detail
