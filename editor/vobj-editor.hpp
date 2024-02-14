#pragma once
#include "src/object-type.hpp"
#include "src/object-id.hpp"
#include <memory>
#include <map>
#include <cr/String.h>

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
    virtual std::shared_ptr<object> make(world& w, object_id id, global_coords pos) const = 0;

    StringView name() const;
    StringView descr() const;
    std::shared_ptr<anim_atlas> atlas() const;
};

class vobj_editor final
{
public:
    struct vobj_ final {
        String name, descr;
        std::unique_ptr<vobj_factory> factory;
    };
    vobj_editor();

    void select_tile(const vobj_& type);
    void clear_selection();
    const vobj_* get_selected() const;

    const vobj_* get_type(StringView name);
    bool is_item_selected(const vobj_& x) const;
    bool is_anything_selected() const;

    static void place_tile(world& w, global_coords pos, const vobj_* x, app& a);

    auto cbegin() const noexcept { return _types.cbegin(); }
    auto cend() const noexcept { return _types.cend(); }
    auto begin() const noexcept { return _types.cbegin(); }
    auto end() const noexcept { return _types.cend(); }

private:
    static std::map<String, vobj_> make_vobj_type_map();

    std::map<String, vobj_> _types = make_vobj_type_map();
    const vobj_* _selected = nullptr;
};

} // namespace floormat
