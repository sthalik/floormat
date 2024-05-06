#include "critter-script.inl"
#include "entity/name-of.hpp"

namespace floormat {

namespace {

struct empty_critter_script final : critter_script
{
    StringView name() const override;
    const void* id() const override;

    void on_init(const std::shared_ptr<critter>& c) override;
    void on_update(const std::shared_ptr<critter>& c, size_t& i, const Ns& dt) override;
    void on_destroy(const std::shared_ptr<critter>& c, script_destroy_reason reason) override;
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

void empty_critter_script::on_init(const std::shared_ptr<critter>&) {}
void empty_critter_script::on_update(const std::shared_ptr<critter>&, size_t&, const Ns&) {}
void empty_critter_script::on_destroy(const std::shared_ptr<critter>&, script_destroy_reason) {}
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
