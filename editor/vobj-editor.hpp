#pragma once
#include "src/object-type.hpp"
#include "src/object-id.hpp"
#include "compat/borrowed-ptr-fwd.hpp"
#include <map>
#include <cr/String.h>
#include <cr/Pointer.h>

namespace floormat {

class world;
struct global_coords;
struct object;
class anim_atlas;
struct vobj_cell;
struct app;

struct vobj_factory
{
    vobj_factory();
    virtual ~vobj_factory() noexcept;
    virtual const vobj_cell& info() const = 0;
    virtual object_type type() const = 0;
    virtual bptr<object> make(world& w, object_id id, global_coords pos) const = 0;

    StringView name() const;
    StringView descr() const;
    bptr<anim_atlas> atlas() const; // todo not thread-safe, copies a non-atomic bptr
};

class vobj_editor final
{
public:
    struct vobj_ final {
        String name, descr;
        Pointer<vobj_factory> factory;
    };
    vobj_editor();

    void select_tile(const vobj_& type);
    void clear_selection();
    const vobj_* get_selected() const;

    const vobj_* get_type(StringView name);
    bool is_item_selected(const vobj_& x) const;
    bool is_anything_selected() const;

    static void place_tile(world& w, global_coords pos, const vobj_* x, app& a);

    typename std::map<String, vobj_>::const_iterator cbegin() const noexcept;
    typename std::map<String, vobj_>::const_iterator cend() const noexcept;
    typename std::map<String, vobj_>::const_iterator begin() const noexcept;
    typename std::map<String, vobj_>::const_iterator end() const noexcept;

private:
    static std::map<String, vobj_> make_vobj_type_map();

    std::map<String, vobj_> _types = make_vobj_type_map();
    const vobj_* _selected = nullptr;
};

} // namespace floormat
