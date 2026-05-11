#pragma once
#include "compat/safe-ptr.hpp"
#include "compat/base-of.hpp"
#include "compat/borrowed-ptr.hpp"
#include "chunk.hpp"
#include "global-coords.hpp"
#include "object-type.hpp"
#include "scenery-type.hpp"
#include "loader/policy.hpp"

namespace floormat::Grid::Pass { class Pool; class PoolRegistry; }
namespace floormat::detail { class chunk_table; }

namespace floormat {

struct object;
struct critter;
struct critter_proto;
struct scenery;
struct scenery_proto;

class world final
{
public:
    static constexpr object_id object_counter_init = 1024;
    static constexpr size_t initial_collect_every = 64;

private:
    struct chunk_tuple
    {
        static constexpr chunk_coords_ invalid_coords = { -1 << 15, -1 << 15, chunk_z_min };
        chunk* c = nullptr;
        chunk_coords_ pos = invalid_coords;
    } _last_chunk;

    struct unique_id : bptr_base
    {
        bool operator==(const unique_id& other) const;
    };

    struct object_id_hasher { size_t operator()(object_id id) const noexcept; };

    struct Impl;
    safe_ptr<Impl> impl;
    safe_ptr<detail::chunk_table> _chunk_table;
    chunk* _head = nullptr;
    chunk* _tail = nullptr;

    bptr<unique_id> _unique_id;
    object_id _object_counter = object_counter_init;
    uint64_t _current_frame = 1; // zero is special for struct object
    bool _teardown : 1 = false;
    bool _script_initialized : 1 = false;
    bool _script_finalized : 1 = false;

    void do_make_object(const bptr<object>& e, global_coords pos, bool sorted); // todo replace 2nd arg with chunk&
    void erase_object(object_id id);
    bptr<object> find_object_(object_id id);

    void register_chunk(chunk* c) noexcept;
    void unregister_chunk(chunk* c) noexcept;

    [[noreturn]] static void throw_on_wrong_object_type(object_id id, object_type actual, object_type expected);
    [[noreturn]] static void throw_on_wrong_scenery_type(object_id id, scenery_type actual, scenery_type expected);
    [[noreturn]] static void throw_on_empty_scenery_proto(object_id id, global_coords pos, Vector2b offset);

    friend struct object;
    friend class chunk;

public:
    explicit world();
    ~world() noexcept;

    chunk& operator[](chunk_coords_ c) noexcept;
    chunk* at(chunk_coords_ c) noexcept;
    const chunk* at(chunk_coords_ c) const noexcept;
    bool contains(chunk_coords_ c) const noexcept;
    void clear();
    void collect(bool force = false, bool quiet = false);
    size_t size() const noexcept;

    template<typename Chunk>
    class chunks_iterator
    {
        Chunk* _cur = nullptr;
        friend class world;
    public:
        chunks_iterator() noexcept = default;
        chunks_iterator& operator++() noexcept;
        Chunk& operator*() const noexcept { return *_cur; }
        bool operator==(const chunks_iterator& o) const noexcept { return _cur == o._cur; }
    };

    template<typename Chunk>
    class chunks_range
    {
        Chunk* _head_ = nullptr;
        friend class world;
        chunks_range(Chunk* h) noexcept : _head_{h} {}
    public:
        chunks_iterator<Chunk> begin() const noexcept;
        chunks_iterator<Chunk> end() const noexcept { return {}; }
    };

    chunks_range<chunk> chunks() noexcept;
    chunks_range<const chunk> chunks() const noexcept;

    void serialize(StringView filename);
    static class world deserialize(StringView filename, loader_policy asset_policy) noexcept(false);
    static void deserialize_old(world& w, ArrayView<const char> buf, uint16_t proto,
                                loader_policy asset_policy) noexcept(false);
    auto frame_no() const { return _current_frame; }
    auto increment_frame_no() { return _current_frame++; }

    template<typename T, bool sorted = true, typename... Xs>
    requires requires(chunk& c, Xs&&... xs) {
        T{object_id(), c, forward<Xs>(xs)...};
        std::is_base_of_v<object, T>;
    }
    bptr<T> make_object(object_id id, global_coords pos, Xs&&... xs)
    {
        auto ret = bptr<T>(new T{id, operator[](pos.chunk3()), forward<Xs>(xs)...});
        do_make_object(ret, pos, sorted);
        return ret;
    }

    template<bool sorted = true> bptr<scenery> make_scenery(object_id id, global_coords pos, scenery_proto&& proto);

    template<typename T = object> bptr<T> find_object(object_id id);
    template<typename T> requires is_strict_base_of<scenery, T> bptr<T> find_object(object_id id);
    template<typename T = object> bptr<const T> find_object(object_id id) const;
    template<typename T> requires is_strict_base_of<scenery, T> bptr<const T> find_object(object_id id) const;

    bptr<critter> ensure_player_character(object_id& id, critter_proto p);
    bptr<critter> ensure_player_character(object_id& id);
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
    std::array<const chunk*, 8> neighbors(chunk_coords_ coord) const;

    void chunk_table_prepare_frame();

    Grid::Pass::Pool& raycast_pass_pool();
    Grid::Pass::PoolRegistry& pass_pool_registry();
    Grid::Pass::Pool& cover_pass_pool();

    static constexpr std::array<Vector2b, 8> neighbor_offsets = {{
        {-1, -1}, {-1,  0}, { 0, -1}, { 1,  1}, { 1,  0}, { 0,  1}, { 1, -1}, {-1,  1},
    }};

    world& operator=(world&& w) noexcept;
    world(world&& w) noexcept;
};

} // namespace floormat
