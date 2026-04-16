#pragma once
#include <cmath>

namespace floormat::Hash {
[[nodiscard]] float open_addressing_load_factor(size_t element_count);
[[nodiscard]] float separate_chaining_load_factor(size_t element_count);
} // namespace floormat::Hash

namespace floormat::Hash::detail {

template<typename T>
concept HashTableC = requires (T& hash, const T::key_type& key) {
    typename T::value_type;
    { hash[key] };
    { hash.size() };
    { hash.max_load_factor(1uz) };
    { hash.reserve(1uz ) };
};

template<typename T>
float set_hash_table_buckets(T& hash_table, float factor, size_t element_count)
{
    size_t buckets = (size_t)std::ceil((float)element_count / factor);
    hash_table.max_load_factor(factor);
    if (hash_table.bucket_count() < buckets)
        hash_table.rehash(buckets);
    return factor;
}

} // namespace floormat::Hash::detail

namespace floormat::Hash {

float set_open_addressing_load_factor(detail::HashTableC auto& hash_table, size_t element_count)
{ return detail::set_hash_table_buckets(hash_table, open_addressing_load_factor(element_count), element_count); }
float set_open_addressing_load_factor(detail::HashTableC auto& hash_table)
{ return set_open_addressing_load_factor(hash_table, hash_table.size()); }

float set_separate_chaining_load_factor(detail::HashTableC auto& hash_table, size_t element_count)
//{ return detail::set_hash_table_buckets(hash_table, separate_chaining_load_factor(element_count), element_count); }
{ return detail::set_hash_table_buckets(hash_table, open_addressing_load_factor(element_count), element_count); }
float set_separate_chaining_load_factor(detail::HashTableC auto& hash_table)
{ return set_separate_chaining_load_factor(hash_table, hash_table.size()); }

} // namespace floormat::Hash
