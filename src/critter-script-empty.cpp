#include "critter-script.inl"
#include "entity/name-of.hpp"

namespace floormat {

namespace {

struct empty_critter_script final : critter_script
{
    StringView name() const override;
    const void* id() const override;

    void on_init(const bptr<critter>& c) override;
    void on_think(const critter& c, const Ns& dt, think_context& ctx) override;
    void on_order(const bptr<critter>& c, usize& i, const Ns& dt, const order& o) override;
    void on_destroy(const bptr<critter>& c, script_destroy_reason reason) override;
    void delete_self() noexcept override;
};

constexpr StringView script_name = name_of<empty_critter_script>;

StringView empty_critter_script::name() const
{
    return "empty"_s;
}

const void* empty_critter_script::id() const
{
    return &script_name;
}

void empty_critter_script::on_init(const bptr<critter>&) {}
void empty_critter_script::on_think(const critter&, const Ns&, think_context&) {}
void empty_critter_script::on_order(const bptr<critter>&, usize&, const Ns&, const order&) { fm_assert(false); }
void empty_critter_script::on_destroy(const bptr<critter>&, script_destroy_reason) {}
void empty_critter_script::delete_self() noexcept { }

constinit empty_critter_script empty_script_ = {};

} // namespace

template <>
critter_script* Script<critter_script, critter>::make_empty()
{
    return &empty_script_;
}

template class Script<critter_script, critter>;

critter_script::~critter_script() noexcept = default;

} // namespace floormat
