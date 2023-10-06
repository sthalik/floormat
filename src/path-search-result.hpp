#pragma once
#include "src/global-coords.hpp"
#include <memory>
#include <vector>

namespace floormat {

struct path_search_result final
{
    friend class path_search;
    friend struct test_app;

    const global_coords* data() const;
    const global_coords& operator[](size_t index) const;
    size_t size() const;

    explicit operator ArrayView<const global_coords>() const;
    explicit operator bool() const;

private:
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(path_search_result);
    path_search_result(const path_search_result& x) noexcept;
    path_search_result& operator=(const path_search_result& x) noexcept;

    static constexpr size_t min_length = TILE_MAX_DIM*2;

    struct node
    {
        friend struct path_search_result;

        node() noexcept;
        fm_DECLARE_DELETED_COPY_ASSIGNMENT(node);
        fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(node);

        std::vector<global_coords> vec;

    private:
        std::unique_ptr<node> _next;
    };

    static std::unique_ptr<node> _pool; // NOLINT(*-avoid-non-const-global-variables)

    path_search_result();
    ~path_search_result() noexcept;

    std::unique_ptr<node> _node;
};

} // namespace floormat
