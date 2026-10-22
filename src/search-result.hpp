#pragma once
#include "point.hpp"
#include <cr/Pointer.h>

namespace floormat {

template<typename T> struct path_search_result_pool_access;

struct path_search_result final
{
    template<typename T> friend struct path_search_result_pool_access;

    struct node;

    struct waypoint
    {
        point pt;
        // The raw_path() segment whose step string pulling was at when it added the waypoint.
        // 0 for the first waypoint, raw_path().size() - 1 for the last.
        uint32_t raw_index;
    };

    size_t size() const;
    bool empty() const;
    uint32_t cost() const;
    void set_cost(uint32_t value);
    float time() const;
    void set_time(float time);
    bool is_found() const;
    void set_found(bool value);
    uint32_t distance() const;
    void set_distance(uint32_t dist);
    uint32_t div_size() const;
    void set_div_size(uint32_t value);

    Array<point>& raw_path();
    ArrayView<const point> path() const;
    // Empty when the path wasn't smoothed. Otherwise from the path's first point to its last.
    Array<waypoint>& smoothings();
    ArrayView<const waypoint> smoothings() const;
    explicit operator bool() const;

    path_search_result();
    path_search_result(path_search_result&&) noexcept;
    path_search_result& operator=(path_search_result&&) noexcept;
    ~path_search_result() noexcept;

private:
    void allocate_node();

    Pointer<node> _node;
    float _time = 0;
    uint32_t _cost = 0, _distance = (uint32_t)-1, _div_size = 0;
    bool _found : 1 = false;

    static Pointer<node> _pool; // NOLINT(*-avoid-non-const-global-variables) todo thread safety
};

} // namespace floormat
