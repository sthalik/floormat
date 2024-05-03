#pragma once
#include "compat/safe-ptr.hpp"
#include "compat/base-of.hpp"
#include "chunk.hpp"
#include "global-coords.hpp"
#include "object-type.hpp"
#include "scenery-type.hpp"
#include "loader/policy.hpp"
#include <memory>
#include <unordered_map>

namespace floormat {

template<typename T> struct shared_ptr_wrapper;
struct object;
struct critter;
struct critter_proto;
struct scenery;
struct scenery_proto;

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
    std::shared_ptr<char> _unique_id = std::make_shared<char>('A');
    object_id _object_counter = object_counter_init;
    uint64_t _current_frame = 1; // zero is special for struct object
    bool _teardown : 1 = false;
    bool _script_initialized : 1 = false;
    bool _script_finalized : 1 = false;

    explicit world(size_t capacity);

    void do_make_object(const std::shared_ptr<object>& e, global_coords pos, bool sorted); // todo! replace 2nd arg with chunk&
    void do_kill_object(object_id id);
    std::shared_ptr<object> find_object_(object_id id);

    [[noreturn]] static void throw_on_wrong_object_type(object_id id, object_type actual, object_type expected);
    [[noreturn]] static void throw_on_wrong_scenery_type(object_id id, scenery_type actual, scenery_type expected);
    [[noreturn]] static void throw_on_empty_scenery_proto(object_id id, global_coords pos, Vector2b offset);

    friend struct object;

public:
    explicit world();
    ~world() noexcept;
    explicit world(std::unordered_map<chunk_coords_, chunk>&& chunks);

    struct pair_chunk_tile final { chunk& c; tile_ref t; }; // NOLINT

    chunk& operator[](chunk_coords_ c) noexcept;
    pair_chunk_tile operator[](global_coords pt) noexcept; // todo maybe remove this overload?
    chunk* at(chunk_coords_ c) noexcept;
    bool contains(chunk_coords_ c) const noexcept;
    void clear();
    void collect(bool force = false);
    size_t size() const noexcept { return _chunks.size(); }

    const auto& chunks() const noexcept { return _chunks; }
    auto& chunks() noexcept { return _chunks; }

    void serialize(StringView filename);
    static class world deserialize(StringView filename, loader_policy asset_policy) noexcept(false);
    static void deserialize_old(class world& w, ArrayView<const char> buf, uint16_t proto, enum loader_policy asset_policy) noexcept(false);
    auto frame_no() const { return _current_frame; }
    auto increment_frame_no() { return _current_frame++; }

    template<typename T, bool sorted = true, typename... Xs>
    requires requires(chunk& c) {
        T{object_id(), c, std::declval<Xs&&>()...};
        std::is_base_of_v<object, T>;
    }
    std::shared_ptr<T> make_object(object_id id, global_coords pos, Xs&&... xs)
    {
        auto ret = std::shared_ptr<T>(new T{id, operator[](pos.chunk3()), forward<Xs>(xs)...});
        do_make_object(std::static_pointer_cast<object>(ret), pos, sorted);
        return ret;
    }

    template<bool sorted = true> std::shared_ptr<scenery> make_scenery(object_id id, global_coords pos, scenery_proto&& proto);
    template<typename T = object> std::shared_ptr<T> find_object(object_id id);
    template<typename T> requires is_strict_base_of<scenery, T> std::shared_ptr<T> find_object(object_id id);

    shared_ptr_wrapper<critter> ensure_player_character(object_id& id, critter_proto p);
    shared_ptr_wrapper<critter> ensure_player_character(object_id& id);
    static critter_proto make_player_proto();

    struct script_status { bool initialized, finalized; };
    void init_scripts();
    void finish_scripts();
    struct script_status script_status() const;

    bool is_teardown() const;
    object_id object_counter() const { return _object_counter; }
    [[nodiscard]] object_id make_id() { return ++_object_counter; }
    void set_object_counter(object_id value);

    std::array<chunk*, 8> neighbors(chunk_coords_ coord);

    static constexpr std::array<Vector2b, 8> neighbor_offsets = {{
        {-1, -1}, {-1,  0}, { 0, -1}, { 1,  1}, { 1,  0}, { 0,  1}, { 1, -1}, {-1,  1},
    }};

    world& operator=(world&& w) noexcept;
    world(world&& w) noexcept;
};

} // namespace floormat
