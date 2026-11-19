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
        // j for the segment from raw_path()[j] to raw_path()[j + 1] holding the point string_pull() tested when it
        // added this waypoint. The first holds 0 and the last raw_path().size() - 1, the index of the last point.
        u32 raw_index;
    };

    usize size() const;
    bool empty() const;
    u32 cost() const;
    void set_cost(u32 value);
    float time() const;
    void set_time(float time);
    bool is_found() const;
    void set_found(bool value);
    u32 distance() const;
    void set_distance(u32 dist);
    u32 div_size() const;
    void set_div_size(u32 value);

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
    u32 _cost = 0, _distance = (u32)-1, _div_size = 0;
    bool _found : 1 = false;

    static Pointer<node> _pool; // NOLINT(*-avoid-non-const-global-variables) todo thread safety
};

} // namespace floormat
