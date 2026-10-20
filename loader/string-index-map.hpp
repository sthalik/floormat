#pragma once
#include <cr/Pointer.h>
#include <cr/StringView.h>

namespace floormat::loader_detail {

// Keeps gtl/phmap.hpp out of the loader headers.
struct string_index_map final
{
    string_index_map();
    ~string_index_map() noexcept;
    string_index_map(const string_index_map&) = delete;
    string_index_map& operator=(const string_index_map&) = delete;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] bool contains(StringView name) const;
    [[nodiscard]] const size_t* find(StringView name) const;
    size_t& operator[](StringView name);
    void clear();
    void set_load_factor(size_t element_count);
    void set_load_factor();
    // -1uz marks a name that has no backing cell
    [[nodiscard]] bool all_indices_below(size_t limit) const;

private:
    struct impl;
    Pointer<impl> p;
};

} // namespace floormat::loader_detail
