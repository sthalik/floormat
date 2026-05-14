#pragma once
#include "chunk.hpp"

namespace floormat {

class const_objects_view final
{
    const bptr<object>* _begin;
    const bptr<object>* _end;

public:
    const_objects_view(const bptr<object>* begin, const bptr<object>* end) noexcept;

    struct iterator
    {
        const bptr<object>* p;

        const object& operator*() const noexcept;
        const object* operator->() const noexcept;
        iterator& operator++() noexcept;
        bool operator==(const iterator&) const noexcept;
    };

    iterator begin() const noexcept;
    iterator end() const noexcept;
    size_t size() const noexcept;
    const object& operator[](size_t i) const noexcept;
};

} // namespace floormat
