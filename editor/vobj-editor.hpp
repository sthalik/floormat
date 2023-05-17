#pragma once
#include "src/entity-type.hpp"
#include <memory>
//#include <Corrade/Containers/ArrayView.h>

namespace Corrade::Containers {
template<typename T> class BasicStringView;
using StringView = BasicStringView<const char>;
template<typename T> class ArrayView;
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
    constexpr vobj_factory() = default;
    virtual constexpr ~vobj_factory() noexcept;
    virtual const vobj_info& info() const = 0;
    virtual entity_type type() const = 0;
    virtual std::shared_ptr<entity> make(world& w, object_id id, global_coords pos) const = 0;

    StringView name() const;
    StringView descr() const;
    std::shared_ptr<anim_atlas> atlas() const;
};

constexpr vobj_factory::~vobj_factory() noexcept {}; // NOLINT workaround gcc 12 bug #93413

#if defined __clang__ || defined __CLION_IDE__
#pragma clang diagnostic pop
#endif

struct vobj_editor final
{
    vobj_editor();

    void select_type(entity_type type);
    void clear_selection();

    [[nodiscard]] static const vobj_factory* get_factory(entity_type type);
    bool is_type_selected(entity_type type) const;
    bool is_anything_selected() const;

    static void place_tile(world& w, global_coords pos, const vobj_factory& factory);

    static auto cbegin() noexcept { return _types.cbegin(); }
    static auto cend() noexcept { return _types.cend(); }
    static auto begin() noexcept { return _types.cbegin(); }
    static auto end() noexcept { return _types.cend(); }

private:
    static const ArrayView<const vobj_factory* const> _types;
    const vobj_factory* _selected = nullptr;
};

} // namespace floormat
