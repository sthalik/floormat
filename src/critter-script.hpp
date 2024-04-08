#pragma once
#include "script.hpp"
#include <memory>

namespace floormat {

struct critter;
struct Ns;

struct critter_script : base_script
{
    critter_script(const std::shared_ptr<critter>& c);
    ~critter_script() noexcept override;

    virtual void on_init(const std::shared_ptr<critter>& c) = 0;
    virtual void on_update(const std::shared_ptr<critter>& c, size_t& i, const Ns& dt) = 0;
    virtual void on_destroy(const std::shared_ptr<critter>& c, script_destroy_reason reason) = 0;
    virtual void delete_self() = 0;
    // todo can_activate, activate

    static critter_script* const empty_script;
};

} // namespace floormat
