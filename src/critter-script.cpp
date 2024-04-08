#include "critter-script.inl"
#include "compat/assert.hpp"

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
    empty_critter_script();
    void on_init(const std::shared_ptr<critter>& c) override;
    void on_update(const std::shared_ptr<critter>& c, size_t& i, const Ns& dt) override;
    void on_destroy(const std::shared_ptr<critter>& c, script_destroy_reason reason) override;
    void delete_self() noexcept override;
};

empty_critter_script::empty_critter_script() : critter_script{nullptr} {}
void empty_critter_script::on_init(const std::shared_ptr<critter>& p)
{
    DBG_nospace << "> init critter:" << (void*)&*p << " id:" << p->id << (p->name ? " name:" : "") << p->name;
    touch_ptr(p);
}
void empty_critter_script::on_update(const std::shared_ptr<critter>& p, size_t&, const Ns&)
{
    DBG_nospace << "> update critter:" << (void*)&*p << " id:" << p->id << (p->name ? " name:" : "") << p->name;
    touch_ptr(p);
}
void empty_critter_script::on_destroy(const std::shared_ptr<critter>& p, script_destroy_reason r)
{
    DBG_nospace << "> destroy critter:" << (void*)&*p << " id:" << p->id << " reason:" << (int)r << (p->name ? " name:" : "") << p->name;
    touch_ptr(p);
}
void empty_critter_script::delete_self() noexcept
{
    DBG_nospace << "> delete critter:";
}

empty_critter_script empty_script_ = {};

} // namespace

critter_script* const critter_script::empty_script = &empty_script_;

critter_script::critter_script(const std::shared_ptr<critter>&) {}
critter_script::~critter_script() noexcept {}

template class Script<critter_script, critter>;

} // namespace floormat
