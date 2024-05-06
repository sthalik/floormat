#pragma once
#include "compat/vector-wrapper-fwd.hpp"
#include "src/global-coords.hpp"
#include <cr/Pointer.h>

namespace floormat {

struct point;
template<typename T> struct path_search_result_pool_access;

struct path_search_result final
{
    template<typename T> friend struct path_search_result_pool_access;

    struct node;

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

    vector_wrapper<point, vector_wrapper_repr::ref> raw_path();
    ArrayView<const point> path() const;
    explicit operator bool() const;

    path_search_result();
    path_search_result(const path_search_result& x) noexcept;
    path_search_result& operator=(const path_search_result& x) noexcept;
    path_search_result(path_search_result&&) noexcept;
    path_search_result& operator=(path_search_result&&) noexcept;
    ~path_search_result() noexcept;

private:
    void allocate_node();

    Pointer<node> _node;
    float _time = 0;
    uint32_t _cost = 0, _distance = (uint32_t)-1;
    bool _found : 1 = false;

    static Pointer<node> _pool; // NOLINT(*-avoid-non-const-global-variables)
};

} // namespace floormat
