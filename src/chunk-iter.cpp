#include "chunk-iter.hpp"

namespace floormat {

const_objects_view::const_objects_view(const bptr<object>* begin, const bptr<object>* end) noexcept
    : _begin{begin}, _end{end}
{
}

const object& const_objects_view::iterator::operator*() const noexcept { return **p; }
const object* const_objects_view::iterator::operator->() const noexcept { return &**p; }

const_objects_view::iterator& const_objects_view::iterator::operator++() noexcept
{
    ++p;
    return *this;
}

bool const_objects_view::iterator::operator==(const iterator& other) const noexcept
{
    return p == other.p;
}

const_objects_view::iterator const_objects_view::begin() const noexcept { return {_begin}; }
const_objects_view::iterator const_objects_view::end() const noexcept { return {_end}; }
size_t const_objects_view::size() const noexcept { return size_t(_end - _begin); }
const object& const_objects_view::operator[](size_t i) const noexcept { return *_begin[i]; }

} // namespace floormat
