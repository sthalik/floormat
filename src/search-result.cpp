#include "search-result.hpp"
//#include "search.hpp"
#include "compat/assert.hpp"
#include "compat/vector-wrapper.hpp"
#include "search-node.hpp"
#include "src/point.inl"
#include <cr/ArrayView.h>
#include <mg/Functions.h>

namespace floormat {

Pointer<path_search_result::node> path_search_result::_pool; // NOLINT

path_search_result::path_search_result() = default;

path_search_result::~path_search_result() noexcept
{
    if (_node && _node->vec.capacity() > 0)
    {
        _node->vec.clear();
        _node->_next = move(_pool);
        _pool = move(_node);
    }
}

path_search_result::path_search_result(const path_search_result& x) noexcept
{
    fm_debug_assert(x._node);
    auto self = path_search_result{};
    self._node->vec = x._node->vec;
    _node = move(self._node);
    _cost = x._cost;
}

path_search_result& path_search_result::operator=(const path_search_result& x) noexcept
{
    fm_debug_assert(_node);
    fm_debug_assert(!_node->_next);
    if (&x != this) [[likely]]
        _node->vec = x._node->vec;
    _cost = x._cost;
    return *this;
}

path_search_result::path_search_result(path_search_result&& other) noexcept
{
    *this = move(other);
}

path_search_result& path_search_result::operator=(path_search_result&& other) noexcept
{
    if (_node)
    {
        _node->vec.clear();
        _node->_next = move(_pool);
        _pool = move(_node);
    }
    _node = move(other._node);
    _time = other._time;
    _cost = other._cost;
    _distance = other._distance;
    _found = other._found;
    return *this;
}

void path_search_result::allocate_node()
{
    constexpr auto min_length = TILE_MAX_DIM*2;

    if (_node)
        return;

    if (_pool)
    {
        auto ptr = move(_pool);
        fm_debug_assert(ptr->vec.empty());
        auto next = move(ptr->_next);
        _node = move(ptr);
        _pool = move(next);
    }
    else
    {
        _node = Pointer<node>{InPlaceInit};
        _node->vec.reserve(min_length);
    }
}

ArrayView<const point> path_search_result::path() const
{
    if (!_node)
        return {};
    return { _node->vec.data(), _node->vec.size() };
}

vector_wrapper<point, vector_wrapper_repr::ref> path_search_result::raw_path()
{
    if (!_node)
        allocate_node();
    return {_node->vec};
}

size_t path_search_result::size() const
{
    return _node ? _node->vec.size() : 0;
}

bool path_search_result::empty() const
{
    return !_node || _node->vec.empty();
}

path_search_result::operator bool() const
{
    return _node && !_node->vec.empty();
}

path_search_result::node::node() noexcept = default;
float path_search_result::time() const { return _time; }
uint32_t path_search_result::cost() const { return _cost; }
void path_search_result::set_cost(uint32_t value) { _cost = value; }
void path_search_result::set_time(float time) { _time = time; }
bool path_search_result::is_found() const { return _found; }
void path_search_result::set_found(bool value) { _found = value; }
uint32_t path_search_result::distance() const { return _distance; }
void path_search_result::set_distance(uint32_t dist) { _distance = dist; }

} // namespace floormat
