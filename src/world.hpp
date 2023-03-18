#pragma once
#include "compat/int-hash.hpp"
#include "compat/defs.hpp"
#include "chunk.hpp"
#include "global-coords.hpp"
#include "entity-type.hpp"
#include "compat/exception.hpp"
#include <unordered_map>
#include <memory>

namespace floormat {

struct entity;
template<typename T> struct entity_type_;

struct world final
{
private:
    struct chunk_tuple final {
        static constexpr chunk_coords invalid_coords = { -1 << 15, -1 << 15 };
        chunk* c = nullptr;
        chunk_coords pos = invalid_coords;
    } _last_chunk;

    static constexpr size_t initial_capacity = 64;
    static constexpr float max_load_factor = .5;
    static constexpr auto hasher = [](chunk_coords c) constexpr -> size_t {
        return int_hash((size_t)c.y << 16 | (size_t)c.x);
    };
    std::unordered_map<chunk_coords, chunk, decltype(hasher)> _chunks;
    std::unordered_map<object_id, std::weak_ptr<entity>> _entities;
    size_t _last_collection = 0;
    size_t _collect_every = 64;
    std::shared_ptr<char> _unique_id = std::make_shared<char>('A');
    object_id _entity_counter = 0;
    bool _teardown : 1 = false;

    explicit world(size_t capacity);

    void do_make_entity(const std::shared_ptr<entity>& e, global_coords pos, bool sorted);
    void do_kill_entity(object_id id);
    std::shared_ptr<entity> find_entity_(object_id id);

    friend struct entity;

public:
    explicit world();
    ~world() noexcept;

    struct pair final { chunk& c; tile_ref t; }; // NOLINT

    template<typename Hash, typename Alloc, typename Pred>
    explicit world(std::unordered_map<chunk_coords, chunk, Hash, Alloc, Pred>&& chunks);

    chunk& operator[](chunk_coords c) noexcept;
    pair operator[](global_coords pt) noexcept;
    bool contains(chunk_coords c) const noexcept;
    void clear();
    void collect(bool force = false);
    void maybe_collect();
    size_t size() const noexcept { return _chunks.size(); }

    const auto& chunks() const noexcept { return _chunks; }

    void serialize(StringView filename);
    static world deserialize(StringView filename);
    void set_collect_threshold(size_t value) { _collect_every = value; }
    size_t collect_threshold() const noexcept { return _collect_every; }

    template<typename T, bool sorted = true, typename... Xs>
    requires requires(chunk& c) { T{object_id(), c, entity_type(), std::declval<Xs>()...}; }
    std::shared_ptr<T> make_entity(object_id id, global_coords pos, Xs&&... xs)
    {
        static_assert(std::is_base_of_v<entity, T>);
        auto ret = std::shared_ptr<T>(new T{id, operator[](pos.chunk()), entity_type_<T>::value, std::forward<Xs>(xs)...});
        do_make_entity(std::static_pointer_cast<entity>(ret), pos, sorted);
        return ret;
    }

    template<typename T = entity>  std::shared_ptr<T> find_entity(object_id id);

    bool is_teardown() const { return _teardown; }
    object_id entity_counter() const { return _entity_counter; }
    [[nodiscard]] object_id make_id() { return ++_entity_counter; }
    void set_entity_counter(object_id value);

    world& operator=(world&& w) noexcept;
    world(world&& w) noexcept;

    fm_DECLARE_DEPRECATED_COPY_ASSIGNMENT(world);
};

template<typename Hash, typename Alloc, typename Pred>
world::world(std::unordered_map<chunk_coords, chunk, Hash, Alloc, Pred>&& chunks) :
    world{std::max(initial_capacity, size_t(1/max_load_factor * 2 * chunks.size()))}
{
    for (auto&& [coord, c] : chunks)
        operator[](coord) = std::move(c);
}

template<typename T>
std::shared_ptr<T> world::find_entity(object_id id)
{
    static_assert(std::is_base_of_v<entity, T>);
    // make it a dependent name so that including "src/entity.hpp" isn't needed
    using U = std::conditional_t<std::is_same_v<T, entity>, T, entity>;
    if (std::shared_ptr<U> ptr = find_entity_(id); !ptr)
        return {};
    else if constexpr(std::is_same_v<T, entity>)
        return ptr;
    else
    {
        fm_soft_assert(ptr->type == entity_type_<T>::value);
        return std::static_pointer_cast<T>(std::move(ptr));
    }
}

} // namespace floormat
