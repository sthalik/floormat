#include "search-result.hpp"
//#include "search.hpp"
#include "compat/assert.hpp"
#include "compat/vector-wrapper.hpp"
#include "search-node.hpp"
#include "src/point.hpp"
#include <Corrade/Containers/ArrayView.h>
#include <utility>

namespace floormat {

namespace {

constexpr size_t min_length = TILE_MAX_DIM*2;

} // namespace

Pointer<path_search_result::node> path_search_result::_pool; // NOLINT

path_search_result::path_search_result()
{
    if (_pool)
    {
        auto ptr = std::move(_pool);
        fm_debug_assert(ptr->vec.empty());
        auto next = std::move(ptr->_next);
        _node = std::move(ptr);
        _pool = std::move(next);
    }
    else
    {
        _node = Pointer<node>{InPlaceInit};
        _node->vec.reserve(min_length);
    }
}

path_search_result::~path_search_result() noexcept
{
    if (_node && _node->vec.capacity() > 0) [[likely]]
    {
        _node->vec.clear();
        _node->_next = std::move(_pool);
        _pool = std::move(_node);
    }
}

path_search_result::path_search_result(const path_search_result& x) noexcept
{
    fm_debug_assert(x._node);
    auto self = path_search_result{};
    self._node->vec = x._node->vec;
    _node = std::move(self._node);
    _cost = x._cost;
}

path_search_result& path_search_result::operator=(const path_search_result& x) noexcept
{
    fm_debug_assert(_node);
    fm_debug_assert(!_node->_next);
    if (&x != this)
        _node->vec = x._node->vec;
    _cost = x._cost;
    return *this;
}

path_search_result::path_search_result(path_search_result&&) noexcept = default;
path_search_result& path_search_result::operator=(path_search_result&&) noexcept = default;

size_t path_search_result::size() const { return _node->vec.size(); }
path_search_result::node::node() noexcept = default;
float path_search_result::time() const { return _time; }

uint32_t path_search_result::cost() const { return _cost; }
void path_search_result::set_cost(uint32_t value) { _cost = value; }
void path_search_result::set_time(float time) { _time = time; }
bool path_search_result::is_found() const { return _found; }
void path_search_result::set_found(bool value) { _found = value; }
uint32_t path_search_result::distance() const { return _distance; }
void path_search_result::set_distance(uint32_t dist) { _distance = dist; }

auto path_search_result::data() const -> const point* { return _node->vec.data(); }
path_search_result::operator bool() const { return !_node->vec.empty(); }

path_search_result::operator ArrayView<const point>() const
{
    fm_debug_assert(_node);
    return {_node->vec.data(), _node->vec.size()};
}

const point& path_search_result::operator[](size_t index) const
{
    fm_debug_assert(_node);
    fm_debug_assert(index < _node->vec.size());
    return data()[index];
}
vector_wrapper<point, vector_wrapper_repr::ref> path_search_result::raw_path() { fm_assert(_node); return {_node->vec}; }
ArrayView<const point> path_search_result::path() const { fm_assert(_node); return {_node->vec.data(), _node->vec.size()}; }

} // namespace floormat