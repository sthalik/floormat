#pragma once
#include "compat/defs.hpp"
#include "chunk.hpp"
#include "global-coords.hpp"
#include "entity-type.hpp"
#include "compat/int-hash.hpp"
#include <memory>
#include <unordered_map>
#include <tsl/robin_map.h>

template<>
struct std::hash<floormat::chunk_coords_> final {
    floormat::size_t operator()(const floormat::chunk_coords_& coord) const noexcept;
};

namespace floormat {

struct entity;
template<typename T> struct entity_type_;
struct object_id_hasher {
    size_t operator()(object_id id) const noexcept { return int_hash(id); }
};

struct world final
{
    static constexpr object_id entity_counter_init = 1024;
    static constexpr size_t initial_capacity = 512;
    static constexpr float max_load_factor = .5;
    static constexpr size_t initial_collect_every = 64;

private:
    struct chunk_tuple final {
        static constexpr chunk_coords_ invalid_coords = { -1 << 15, -1 << 15, chunk_min_z };
        chunk* c = nullptr;
        chunk_coords_ pos = invalid_coords;
    } _last_chunk;

    std::unordered_map<chunk_coords_, chunk> _chunks;
    tsl::robin_map<object_id, std::weak_ptr<entity>, object_id_hasher> _entities;
    size_t _last_collection = 0;
    size_t _collect_every = initial_collect_every;
    std::shared_ptr<char> _unique_id = std::make_shared<char>('A');
    object_id _entity_counter = entity_counter_init;
    uint64_t _current_frame = 1; // zero is special for struct entity
    bool _teardown : 1 = false;

    explicit world(size_t capacity);

    void do_make_entity(const std::shared_ptr<entity>& e, global_coords pos, bool sorted);
    void do_kill_entity(object_id id);
    std::shared_ptr<entity> find_entity_(object_id id);
    [[noreturn]] static void throw_on_wrong_entity_type(object_id id, entity_type actual, entity_type expected);

    friend struct entity;

public:
    explicit world();
    ~world() noexcept;
    explicit world(std::unordered_map<chunk_coords_, chunk>&& chunks);

    struct pair final { chunk& c; tile_ref t; }; // NOLINT

    chunk& operator[](chunk_coords_ c) noexcept;
    pair operator[](global_coords pt) noexcept;
    chunk* at(chunk_coords_ c) noexcept;
    bool contains(chunk_coords_ c) const noexcept;
    void clear();
    void collect(bool force = false);
    void maybe_collect();
    size_t size() const noexcept { return _chunks.size(); }

    const auto& chunks() const noexcept { return _chunks; }

    void serialize(StringView filename);
    static world deserialize(StringView filename);
    void set_collect_threshold(size_t value) { _collect_every = value; }
    size_t collect_threshold() const noexcept { return _collect_every; }
    auto frame_no() const { return _current_frame; }
    auto increment_frame_no() { return _current_frame++; }

    template<typename T, bool sorted = true, typename... Xs>
    requires requires(chunk& c) {
        T{object_id(), c, std::declval<Xs>()...};
        std::is_base_of_v<entity, T>;
    }
    std::shared_ptr<T> make_entity(object_id id, global_coords pos, Xs&&... xs)
    {
        auto ret = std::shared_ptr<T>(new T{id, operator[](chunk_coords_{pos.chunk(), pos.z()}), std::forward<Xs>(xs)...});
        do_make_entity(std::static_pointer_cast<entity>(ret), pos, sorted);
        return ret;
    }

    template<typename T = entity> std::shared_ptr<T> find_entity(object_id id);

    bool is_teardown() const { return _teardown; }
    object_id entity_counter() const { return _entity_counter; }
    [[nodiscard]] object_id make_id() { return ++_entity_counter; }
    void set_entity_counter(object_id value);

    world& operator=(world&& w) noexcept;
    world(world&& w) noexcept;

    fm_DECLARE_DEPRECATED_COPY_ASSIGNMENT(world);
};

template<typename T>
std::shared_ptr<T> world::find_entity(object_id id)
{
    static_assert(std::is_same_v<entity, T> || std::is_base_of_v<entity, T>);
    // make it a dependent name so that including "src/entity.hpp" isn't needed
    using U = std::conditional_t<std::is_same_v<T, entity>, T, entity>;
    if (std::shared_ptr<U> ptr = find_entity_(id); !ptr)
        return {};
    else if constexpr(std::is_same_v<T, entity>)
        return ptr;
    else
    {
        if (!(ptr->type() == entity_type_<T>::value)) [[unlikely]]
            throw_on_wrong_entity_type(id, ptr->type(), entity_type_<T>::value);
        return std::static_pointer_cast<T>(std::move(ptr));
    }
}

} // namespace floormat
