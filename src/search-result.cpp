#include "search-result.hpp"
//#include "search.hpp"
#include "compat/assert.hpp"
#include "compat/vector-wrapper.hpp"
#include "search-node.hpp"
#include "src/point.inl"
#include <cr/ArrayView.h>
#include <mg/Functions.h>

namespace floormat {

namespace {

struct off_pair
{
    unsigned val;
    bool is_bad;
    constexpr bool operator==(const off_pair&) const = default;
};

template<unsigned N> constexpr off_pair offset_is_bad(Int x)
{
    switch (x)
    {
    case Int{-1}:
    case Int{ 1}:
    case Int{ 0}:
        return { (unsigned(Int{1} + x)) << N, false };
    default:
        return { (unsigned)-1, true };
    }
}
#if 0
static_assert((2 | 2 << 8) == (offset_is_bad<0>(Int{1}).val | offset_is_bad<8>(Int{1}).val));
static_assert((2 | 1 << 8) == (offset_is_bad<0>(Int{1}).val | offset_is_bad<8>(Int{0}).val));
static_assert((1 | 1 << 8) == (offset_is_bad<0>(Int{0}).val | offset_is_bad<8>(Int{0}).val));
static_assert((0 | 2 << 8) == (offset_is_bad<0>(Int{-1}).val | offset_is_bad<8>(Int{1}).val));
static_assert((unsigned)-1 == (offset_is_bad<0>(Int{4242}).val | offset_is_bad<8>(Int{1}).val));
#endif

void simplify_path(const std::vector<point>& src, std::vector<path_search_result::pair>& dest)
{
    dest.clear();
    dest.reserve(src.size());
    fm_assert(!src.empty());
    fm_assert(src.size() >= 2);
    const auto size = (uint32_t)src.size();
    dest.push_back({src[0], 0});

    auto last = src[0];
    auto cur = src[1] - src[0];
    size_t len = 1;

    for (auto i = 1u; i < size; i++)
    {

    }
}

} // namespace

Pointer<path_search_result::node> path_search_result::_pool; // NOLINT

path_search_result::path_search_result()
{
    constexpr auto min_length = TILE_MAX_DIM*2;
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

path_search_result::~path_search_result() noexcept
{
    if (_node && _node->vec.capacity() > 0) [[likely]]
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
bool path_search_result::is_simplified() const { /*fm_assert(!_node->vec.empty());*/ return !_node->vec_.empty(); }
auto path_search_result::simplified_path() -> ArrayView<const pair>
{
    if (!_node->vec_.empty())
        return { _node->vec_.data(), _node->vec_.size() };
    else
    {
        //fm_assert(!_node->vec.empty());
        simplify_path(_node->vec, _node->vec_);
        fm_assert(!_node->vec_.empty());
        return { _node->vec_.data(), _node->vec_.size() };
    }
}

path_search_result::operator bool() const { return !_node->vec.empty(); }

vector_wrapper<point, vector_wrapper_repr::ref> path_search_result::raw_path() { fm_assert(_node); return {_node->vec}; }
auto path_search_result::raw_simplified_path() -> vector_wrapper<pair, vector_wrapper_repr::ref> { fm_assert(_node); return {_node->vec_}; }
ArrayView<const point> path_search_result::path() const { fm_assert(_node); return {_node->vec.data(), _node->vec.size()}; }

} // namespace floormat
