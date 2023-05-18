#pragma once
#include "src/entity-type.hpp"
#include <memory>
#include <map>

namespace Corrade::Containers {
template<typename T> class BasicStringView;
using StringView = BasicStringView<const char>;
} // namespace Corrade::Containers

namespace floormat {

struct world;
struct global_coords;
struct entity;
struct anim_atlas;
struct vobj_info;

#if defined __clang__ || defined __CLION_IDE__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

struct vobj_factory
{
    vobj_factory();
    virtual ~vobj_factory() noexcept;
    virtual const vobj_info& info() const = 0;
    virtual entity_type type() const = 0;
    virtual std::shared_ptr<entity> make(world& w, object_id id, global_coords pos) const = 0;

    StringView name() const;
    StringView descr() const;
    std::shared_ptr<anim_atlas> atlas() const;
};

#if defined __clang__ || defined __CLION_IDE__
#pragma clang diagnostic pop
#endif

struct vobj_editor final
{
    struct vobj_ final {
        StringView name, descr;
        std::unique_ptr<vobj_factory> factory;
    };

    vobj_editor();

    void select_tile(const vobj_& type);
    void clear_selection();
    const vobj_* get_selected() const;

    const vobj_* get_type(StringView name);
    bool is_item_selected(const vobj_& x) const;
    bool is_anything_selected() const;

    static void place_tile(world& w, global_coords pos, const vobj_* x);

    auto cbegin() const noexcept { return _types.cbegin(); }
    auto cend() const noexcept { return _types.cend(); }
    auto begin() const noexcept { return _types.cbegin(); }
    auto end() const noexcept { return _types.cend(); }

private:
    static std::map<StringView, vobj_> make_vobj_type_map();

    std::map<StringView, vobj_> _types = make_vobj_type_map();
    const vobj_* _selected = nullptr;
};

} // namespace floormat
