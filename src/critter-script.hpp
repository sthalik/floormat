#pragma once
#include "script.hpp"
#include <memory>
#include <cr/Pointer.h>

namespace floormat {

struct critter;
struct Ns;

struct point;
struct path_search_result;

struct critter_script : base_script
{
    constexpr critter_script() noexcept = default;
    ~critter_script() noexcept override;

    virtual void on_init(const std::shared_ptr<critter>& c) = 0;
    // todo can_activate, activate
    virtual void on_update(const std::shared_ptr<critter>& c, size_t& i, const Ns& dt) = 0;
    virtual void on_destroy(const std::shared_ptr<critter>& c, script_destroy_reason reason) = 0;
    virtual void delete_self() = 0;

    // todo! move to src/scripts dir

    object_type type() const override;

    enum class walk_mode : uint8_t { none, line, path, };
    static Pointer<critter_script> make_walk_script(point to, const path_search_result& path, walk_mode mode);
};

} // namespace floormat
