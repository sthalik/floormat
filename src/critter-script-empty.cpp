#include "critter-script.inl"
#include "entity/name-of.hpp"

namespace floormat {

namespace {

CORRADE_ALWAYS_INLINE
void touch_ptr(const std::shared_ptr<critter>& p)
{
    (void)p;
#if fm_ASAN
    volatile char foo = *reinterpret_cast<volatile const char*>(&*p);
    (void)foo;
//#else
//    fm_debug_assert(p);
#endif
}

struct empty_critter_script final : critter_script
{
    const void* type_id() const override;
    void on_init(const std::shared_ptr<critter>& c) override;
    void on_update(const std::shared_ptr<critter>& c, size_t& i, const Ns& dt) override;
    void on_destroy(const std::shared_ptr<critter>& c, script_destroy_reason reason) override;
    void delete_self() noexcept override;
};

constexpr StringView script_name = name_of<empty_critter_script>;

const void* empty_critter_script::type_id() const
{
    return &script_name;
}

void empty_critter_script::on_init(const std::shared_ptr<critter>& p)
{
    DBG_nospace << "> script init critter:" << (void*)&*p << " id:" << p->id << (p->name ? " name:" : "") << p->name;
    touch_ptr(p);
}
void empty_critter_script::on_update(const std::shared_ptr<critter>& p, size_t&, const Ns&)
{
    //DBG_nospace << "  script update critter:" << (void*)&*p << " id:" << p->id << (p->name ? " name:" : "") << p->name;
    touch_ptr(p);
}
void empty_critter_script::on_destroy(const std::shared_ptr<critter>& p, script_destroy_reason r)
{
    DBG_nospace << "  script destroy critter:" << (void*)&*p << " id:" << p->id << " reason:" << (int)r << (p->name ? " name:" : "") << p->name;
    touch_ptr(p);
}
void empty_critter_script::delete_self() noexcept
{
    DBG_nospace << "< script delete critter";
}

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
