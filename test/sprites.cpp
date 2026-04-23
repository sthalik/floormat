#include "app.hpp"
#include "loader/loader.hpp"
#include "src/sprite-atlas-impl.hpp"
#include "src/sprite-atlas.hpp"
#include "src/sprite-constants.hpp"
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
#include <cr/String.h>
#include <cr/Path.h>
#include <mg/ImageData.h>
#include <mg/ImageView.h>
#include <gtl/phmap.hpp>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace floormat::Test {
namespace {

// Identifier used to detect symlink loops while walking.
// On POSIX, (st_dev, st_ino) uniquely identifies a directory across mount
// points. On Windows, MSVC's stat() leaves both fields at 0 — we treat that
// as "no usable identifier" and fall through to the depth limit instead of
// deduping unrelated directories that all happen to map to (0,0).

struct file_id { uint64_t dev, ino; };
struct file_id_hash {
    size_t operator()(const file_id& id) const noexcept {
        return id.dev * 0x9E3779B97F4A7C15ull ^ id.ino;
    }
};
struct file_id_eq {
    bool operator()(const file_id& a, const file_id& b) const noexcept {
        return a.dev == b.dev && a.ino == b.ino;
    }
};
using visited_set = gtl::flat_hash_set<file_id, file_id_hash, file_id_eq>;

constexpr uint32_t max_depth = 64;

// Returns false if we couldn't obtain an identifier (permission denied,
// broken symlink, missing path, etc.). On failure the walker falls back
// to the depth limit as its sole loop guard.
#ifdef _WIN32

bool get_file_id(const String& path, file_id& out)
{
    // Access mask = 0 is sufficient: we don't read/write contents, only
    // query the ID. FILE_FLAG_BACKUP_SEMANTICS is required to open a
    // DIRECTORY handle — without it CreateFileW refuses dirs outright.
    HANDLE h = CreateFileA(path.data(),
                           0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr,
                           OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    // NTFS: 64-bit file index fits. ReFS: file IDs are 128-bit and this
    // call truncates — for a ReFS-hosted corpus we'd need
    // GetFileInformationByHandleEx(FileIdInfo). Not relevant here.
    BY_HANDLE_FILE_INFORMATION info;
    BOOL ok = GetFileInformationByHandle(h, &info);
    CloseHandle(h);
    if (!ok) return false;
    out.dev = (uint64_t)info.dwVolumeSerialNumber;
    out.ino = ((uint64_t)info.nFileIndexHigh << 32) | (uint64_t)info.nFileIndexLow;
    return true;
}

#else

bool get_file_id(const String& path, file_id& out)
{
    struct stat st;
    if (::stat(path.data(), &st) != 0)
        return false;
    out.dev = (uint64_t)st.st_dev;
    out.ino = (uint64_t)st.st_ino;
    return true;
}

#endif

void walk_impl(const String& dir, Array<String>& results,
               visited_set& visited, uint32_t depth)
{
    if (depth > max_depth)
        return;

    // Dedup when the platform handed us a meaningful identifier. Guard the
    // (0,0) case: some filesystems / error paths return zeros for both
    // fields, and inserting on that key would false-match unrelated
    // zero-keyed entries.
    file_id id;
    if (get_file_id(dir, id))
    {
        if (id.dev != 0 || id.ino != 0)
            if (!visited.insert(id).second)
                return;
    }

    auto entries = Utility::Path::list(dir, Utility::Path::ListFlag::SkipDotAndDotDot);
    if (!entries)
        return;

    for (const auto& entry : *entries)
    {
        String full = Utility::Path::join(dir, entry);
        if (Utility::Path::isDirectory(full))
            walk_impl(full, results, visited, depth + 1);
        else
            arrayAppend(results, InPlaceInit, move(full));
    }
}

Array<String> walk_directory_tree(StringView root)
{
    Array<String> results;
    arrayReserve(results, 256);

    visited_set visited;
    visited.reserve(64);

    walk_impl(String{root}, results, visited, 0);
    return results;
}

} // namespace

void test_sprites()
{
    // Skip silently when the corpus isn't pointed at — most dev machines
    // won't have the Fallout 2 PNG dump locally.
    const char* env = std::getenv("PNG_SPRITES_LOCATION");
    if (!env || !*env)
        return;
    const char* output = std::getenv("OUTPUT_PNG_FILE");
    fm_assert(output && *output && "missing env var OUTPUT_PNG_FILE");

    Array<String> files = walk_directory_tree(env);
    fm_assert(files.size() > 0);

    // layer_size matches max_texture_xy in sprite-atlas-impl.hpp — the
    // 10-bit Sprite::width/height storage caps individual sprites at 1024.
    SpriteAtlas::Atlas atlas;
    atlas.layer_size = (uint16_t)Math::min<unsigned>(SpriteAtlas::max_layer_size, SpriteAtlas::max_2d_texture_size());

    uint32_t png_count = 0, alloc_count = 0;
    for (const String& path : files)
    {
        // Walker returns all regular files; filter to PNG here so the walker
        // stays general-purpose.
        if (!StringView{path}.hasSuffix(".png"_s))
            continue;
        png_count++;
        auto img = loader.image(path);
        auto size = img.size();
        // Skip sprites larger than the bitfield limit — a handful of F2
        // worldmap/splash PNGs exceed 1024 in either dimension.
        if ((uint32_t)size.max() > SpriteAtlas::max_texture_xy)
            continue;
        auto* sprite = SpriteAtlas::alloc_sprite(atlas, (uint32_t)size.x(), (uint32_t)size.y());
        SpriteAtlas::upload_sprite(atlas, *sprite, img);
        alloc_count++;
    }

    DBG << "sprites: walked" << files.size()
        << "pngs" << png_count
        << "allocated" << alloc_count
        << "layers" << atlas.n_layers;

    if (!atlas.layers.isEmpty())
    {
        const auto& shelf = *atlas.layers.back().shelves.back();
        DBG_nospace << "sprites: last shelf layer:" << atlas.layers.size()-1 <<  " y:" << shelf.y;
    }

    SpriteAtlas::dump_atlas(atlas, output);
}

} // namespace floormat::Test
