#pragma once
#include "compat/safe-ptr.hpp"
#include "chunk.hpp"
#include "global-coords.hpp"
#include "object-type.hpp"
#include <memory>
#include <unordered_map>
#include <Corrade/Utility/Move.h>

namespace floormat {

struct object;
template<typename T> struct object_type_;

class world final
{
public:
    static constexpr object_id object_counter_init = 1024;
    static constexpr size_t initial_capacity = 4096;
    static constexpr float max_load_factor = .25;
    static constexpr size_t initial_collect_every = 64;

private:
    struct chunk_tuple
    {
        static constexpr chunk_coords_ invalid_coords = { -1 << 15, -1 << 15, chunk_z_min };
        chunk* c = nullptr;
        chunk_coords_ pos = invalid_coords;
    } _last_chunk;

    struct object_id_hasher { size_t operator()(object_id id) const noexcept; };
    struct chunk_coords_hasher { size_t operator()(const chunk_coords_& coord) const noexcept; };
    struct robin_map_wrapper;

    std::unordered_map<chunk_coords_, chunk, chunk_coords_hasher> _chunks;
    safe_ptr<robin_map_wrapper> _objects;
    size_t _last_collection = 0;
    size_t _collect_every = initial_collect_every;
    std::shared_ptr<char> _unique_id = std::make_shared<char>('A');
    object_id _object_counter = object_counter_init;
    uint64_t _current_frame = 1; // zero is special for struct object
    bool _teardown : 1 = false;

    explicit world(size_t capacity);

    void do_kill_object(object_id id);
    std::shared_ptr<object> find_object_(object_id id);
    [[noreturn]] static void throw_on_wrong_object_type(object_id id, object_type actual, object_type expected);

    friend struct object;

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
    static class world deserialize(StringView filename) noexcept(false);
    static void deserialize_old(class world& w, ArrayView<const char> buf, uint16_t proto) noexcept(false);
    void set_collect_threshold(size_t value) { _collect_every = value; }
    size_t collect_threshold() const noexcept { return _collect_every; }
    auto frame_no() const { return _current_frame; }
    auto increment_frame_no() { return _current_frame++; }

    template<typename T, bool sorted = true, typename... Xs>
    requires requires(chunk& c) {
        T{object_id(), c, std::declval<Xs>()...};
        std::is_base_of_v<object, T>;
    }
    std::shared_ptr<T> make_object(object_id id, global_coords pos, Xs&&... xs)
    {
        auto ret = std::shared_ptr<T>(new T{id, operator[](pos.chunk3()), Utility::forward<Xs>(xs)...});
        do_make_object(static_pointer_cast<object>(ret), pos, sorted);
        return ret;
    }
    void do_make_object(const std::shared_ptr<object>& e, global_coords pos, bool sorted);

    template<typename T, typename... Xs> std::shared_ptr<object> make_unconnected_object(Xs&&... xs)
    {
        return std::shared_ptr<T>(new T{0, operator[](chunk_coords_{}), {}, Utility::forward<Xs>(xs)...});
    }

    template<typename T = object> std::shared_ptr<T> find_object(object_id id);

    bool is_teardown() const { return _teardown; }
    object_id object_counter() const { return _object_counter; }
    [[nodiscard]] object_id make_id() { return ++_object_counter; }
    void set_object_counter(object_id value);

    struct neighbor_pair final { chunk* c = nullptr; chunk_coords_ coord; };

    std::array<neighbor_pair, 8> neighbors(chunk_coords_ coord);

    static constexpr std::array<Vector2b, 8> neighbor_offsets = {{
        {-1, -1}, {-1,  0}, { 0, -1}, { 1,  1}, { 1,  0}, { 0,  1}, { 1, -1}, {-1,  1},
    }};

    world& operator=(world&& w) noexcept;
    world(world&& w) noexcept;
};

template<typename T>
std::shared_ptr<T> world::find_object(object_id id)
{
    static_assert(std::is_same_v<object, T> || std::is_base_of_v<object, T>);
    // make it a dependent name so that including "src/object.hpp" isn't needed
    using U = std::conditional_t<std::is_same_v<T, object>, T, object>;
    if (std::shared_ptr<U> ptr = find_object_(id); !ptr)
        return {};
    else if constexpr(std::is_same_v<T, object>)
        return ptr;
    else
    {
        if (!(ptr->type() == object_type_<T>::value)) [[unlikely]]
            throw_on_wrong_object_type(id, ptr->type(), object_type_<T>::value);
        return static_pointer_cast<T>(Utility::move(ptr));
    }
}

} // namespace floormat
