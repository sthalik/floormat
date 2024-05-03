#include "critter-script.hpp"
//#include "raycast.hpp"
#include "point.hpp"
#include "critter.hpp"
#include "search-result.hpp"
#include "search-astar.hpp"
#include "entity/name-of.hpp"

namespace floormat {

namespace {

enum class walk_mode : uint8_t
{
    none, line, path,
};

struct walk_script final : critter_script
{
    const void* type_id() const override;
    void on_init(const std::shared_ptr<critter>& c) override;
    void on_update(const std::shared_ptr<critter>& c, size_t& i, const Ns& dt) override;
    void on_destroy(const std::shared_ptr<critter>& c, script_destroy_reason reason) override;
    void delete_self() noexcept override;

    point to;
    path_search_result path;
    walk_mode mode = walk_mode::none;
};

constexpr StringView script_name = name_of<walk_script>;

const void* walk_script::type_id() const
{
    return &script_name;
}

void walk_script::on_init(const std::shared_ptr<critter>& c)
{
}

void walk_script::on_update(const std::shared_ptr<critter>& c, size_t& i, const Ns& dt)
{
}

void walk_script::on_destroy(const std::shared_ptr<critter>& c, script_destroy_reason reason)
{
}

void walk_script::delete_self() noexcept
{
    delete this;
}

} // namespace

critter_script* critter_script::make_walk_script(point to, path_search_result path)
{
    return new walk_script{};
}

} // namespace floormat
