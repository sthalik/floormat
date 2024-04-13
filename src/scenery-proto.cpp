#include "scenery-proto.hpp"
#include "compat/overloaded.hpp"

namespace floormat {

// ---------- generic_scenery_proto ----------

bool generic_scenery_proto::operator==(const generic_scenery_proto& p) const = default;
enum scenery_type generic_scenery_proto::scenery_type() { return scenery_type::generic; }

// ---------- door_scenery_proto ----------

bool door_scenery_proto::operator==(const door_scenery_proto& p) const = default;
enum scenery_type door_scenery_proto::scenery_type() { return scenery_type::door; }

// --- scenery_proto ---

scenery_proto::scenery_proto() noexcept { type = object_type::scenery; }
scenery_proto::~scenery_proto() noexcept = default;
scenery_proto::operator bool() const { return atlas != nullptr; }

scenery_proto& scenery_proto::operator=(const scenery_proto&) noexcept = default;
scenery_proto::scenery_proto(const scenery_proto&) noexcept = default;
scenery_proto& scenery_proto::operator=(scenery_proto&&) noexcept = default;
scenery_proto::scenery_proto(scenery_proto&&) noexcept = default;

enum scenery_type scenery_proto::scenery_type() const
{
    return std::visit(overloaded {
        [](std::monostate) { return scenery_type::none; },
        []<typename T>(const T&) { return T::scenery_type(); },
        }, subtype
    );
}

bool scenery_proto::operator==(const object_proto& e0) const
{
    if (type != e0.type)
        return false;

    if (!object_proto::operator==(e0))
        return false;

    const auto& sc = static_cast<const scenery_proto&>(e0);

    if (subtype.index() != sc.subtype.index())
        return false;

    return std::visit(
        [](const auto& a, const auto& b) -> bool {
            if constexpr(std::is_same_v<std::decay_t<decltype(a)>, std::decay_t<decltype(b)>>)
                return a == b;
            else
                fm_assert(false);
        },
        subtype, sc.subtype
    );
}

} // namespace floormat
