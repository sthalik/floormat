#include "impl.hpp"
//#include "compat/assert.hpp"
#include "compat/exception.hpp"
#include "loader/loader.hpp"
//#include "src/emplacer.hpp"
//#include "src/anim-atlas.hpp"
#include <cstring>
//#include <cstdio>
//#include <Corrade/Containers/ArrayView.h>
//#include <Corrade/Containers/Pair.h>
//#include <Corrade/Containers/StridedArrayView.h>
//#include <Corrade/Containers/String.h>
//#include <Magnum/Trade/ImageData.h>

namespace floormat {

StringView loader_::make_atlas_path(char(&buf)[fm_FILENAME_MAX], StringView dir, StringView name, StringView ext)
{
    fm_soft_assert(!dir || dir[dir.size()-1] == '/');
    fm_soft_assert(name);
    fm_soft_assert(!ext || ext[0] == '.');
    const auto dirsiz = dir.size(), namesiz = name.size(), extsiz = ext.size(),
               len = dirsiz + namesiz + extsiz;
    fm_soft_assert(len < fm_FILENAME_MAX);
    if (dir)
        std::memcpy(&buf[0], dir.data(), dirsiz);
    std::memcpy(&buf[dirsiz], name.data(), namesiz);
    if (ext)
        std::memcpy(&buf[dirsiz + namesiz], ext.data(), extsiz);
    buf[len] = '\0';
    return StringView{buf, len, StringViewFlag::NullTerminated};
}

bool loader_::check_atlas_name(StringView str) noexcept
{
    constexpr auto first_char =
        "@_0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"_s;
    if (str == loader.INVALID)
        return true;
    if (!str || !first_char.find(str[0]))
        return false;
    if (str.findAny("\\\"'\n\r\t\a\033\0|$!%{}^*?<>&;:^"_s) ||
        str.find("/."_s) || str.find("//"_s) || str.find('\0'))
        return false;

    return true;
}

} // namespace floormat
