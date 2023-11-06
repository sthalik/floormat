#pragma once
#include "src/global-coords.hpp"
#include "compat/defs.hpp"
#include <memory>
#include <vector>

namespace floormat {

struct point;

struct path_search_result final
{
    friend struct test_app;

    const point* data() const;
    const point& operator[](size_t index) const;
    size_t size() const;
    uint32_t cost() const;
    void set_cost(uint32_t value);
    float time() const;
    void set_time(float time);
    bool is_found() const;
    void set_found(bool value);
    uint32_t distance() const;
    void set_distance(uint32_t dist);

    std::vector<point>& path();
    const std::vector<point>& path() const;
    explicit operator ArrayView<const point>() const;
    explicit operator bool() const;

    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(path_search_result);
    path_search_result(const path_search_result& x) noexcept;
    path_search_result& operator=(const path_search_result& x) noexcept;
    path_search_result();
    ~path_search_result() noexcept;

private:
    struct node
    {
        friend struct path_search_result;
        friend struct test_app;

        node() noexcept;
        fm_DECLARE_DELETED_COPY_ASSIGNMENT(node);
        fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(node);

        std::vector<point> vec;

    private:
        std::unique_ptr<node> _next;
    };

    static std::unique_ptr<node> _pool; // NOLINT(*-avoid-non-const-global-variables)

    std::unique_ptr<node> _node;
    float _time = 0;
    uint32_t _cost = 0, _distance = (uint32_t)-1;
    bool _found : 1 = false;
};

} // namespace floormat
