#include <string>
#include <vector>
#include <tuple>
#include <unordered_map>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>

namespace std::filesystem { class path; }

struct big_atlas_tile final {
    Magnum::Vector2i position;
};

struct big_atlas_entry final {
    std::vector<big_atlas_tile> tiles;
};

struct big_atlas final {
    static std::tuple<big_atlas, bool> from_json(const std::filesystem::path& pathname) noexcept;
    [[nodiscard]] bool to_json(const std::filesystem::path& pathname) noexcept;

    std::unordered_map<std::string, big_atlas_entry> entries;
};
