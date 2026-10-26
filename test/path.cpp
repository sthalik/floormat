#include "app.hpp"
#include "compat/path.hpp"
#include <cr/Optional.h>
#include <cr/Path.h>

namespace floormat::Test {

namespace {

#define CHECK_SPLIT(path, head, tail)                                                       \
    (fm_assert_equal(StringView{head}, split(StringView{path}).first()),                    \
     fm_assert_equal(StringView{tail}, split(StringView{path}).second()))

#define CHECK_EXTENDED(path, expected)                                                      \
    do {                                                                                    \
        const String actual = extended_length_path(StringView{path});                       \
        fm_assert_equal(StringView{expected}, StringView{actual});                          \
    } while (false)

#define CHECK_PREFIX(exe, prefix)                                                           \
    do {                                                                                    \
        const String actual = extended_length_path(split(split(StringView{exe}).first()).first()); \
        fm_assert_equal(StringView{prefix}, StringView{actual});                            \
    } while (false)

void test_split_posix()
{
    CHECK_SPLIT("foo/bar///", "foo/bar", "");
    CHECK_SPLIT("foo/bar", "foo", "bar");
    CHECK_SPLIT("foo//bar", "foo", "bar");
    CHECK_SPLIT("/foo/bar", "/foo", "bar");
    CHECK_SPLIT("foo", "", "foo");
    CHECK_SPLIT("foo/", "foo", "");
    CHECK_SPLIT("", "", "");
    CHECK_SPLIT("/", "/", "");
    CHECK_SPLIT("/foo", "/", "foo");
    CHECK_SPLIT("//foo", "//", "foo");
    CHECK_SPLIT("///foo", "///", "foo");
    CHECK_SPLIT("//", "//", "");
    CHECK_SPLIT("///", "///", "");
    CHECK_SPLIT("foo/bar/baz", "foo/bar", "baz");
    CHECK_SPLIT("/foo/bar/", "/foo/bar", "");
    CHECK_SPLIT("//server", "//", "server");
    CHECK_SPLIT("//server/", "//server", "");
}

#ifdef _WIN32

void test_split_drive()
{
    CHECK_SPLIT("C:/foo", "C:/", "foo");
    CHECK_SPLIT("C:/", "C:/", "");
    CHECK_SPLIT("C:\\foo\\bar", "C:\\foo", "bar");
    CHECK_SPLIT("C:foo", "C:", "foo");
    CHECK_SPLIT("C:", "C:", "");
    CHECK_SPLIT("C:/foo/bar/", "C:/foo/bar", "");
    CHECK_SPLIT("c:/Foo/bin/x.exe", "c:/Foo/bin", "x.exe");
    CHECK_SPLIT("C:\\", "C:\\", "");
}

void test_split_extended()
{
    CHECK_SPLIT("\\\\?\\C:/foo/bin", "\\\\?\\C:/foo", "bin");
    CHECK_SPLIT("\\\\?\\C:/bin", "\\\\?\\C:/", "bin");
    CHECK_SPLIT("\\\\?\\C:\\bin", "\\\\?\\C:\\", "bin");
    CHECK_SPLIT("\\\\?\\C:/", "\\\\?\\C:/", "");
    CHECK_SPLIT("//?/C:/foo/bin", "//?/C:/foo", "bin");
    CHECK_SPLIT("//?/C:/bin", "//?/C:/", "bin");
    CHECK_SPLIT("//?/C:/", "//?/C:/", "");
    CHECK_SPLIT("//?/C:/foo/bin/", "//?/C:/foo/bin", "");
    CHECK_SPLIT("//?/C:", "//?/C:", "");
    CHECK_SPLIT("//?/", "//?/", "");
    CHECK_SPLIT("//?/foo", "//?/foo", "");
    CHECK_SPLIT("\\\\?\\c:\\", "\\\\?\\c:\\", "");
    CHECK_SPLIT("\\\\?\\C:\\", "\\\\?\\C:\\", "");
    CHECK_SPLIT("\\\\?\\c:", "\\\\?\\c:", "");
    CHECK_SPLIT("\\\\?\\c:\\bin", "\\\\?\\c:\\", "bin");
    CHECK_SPLIT("\\\\?\\c:/", "\\\\?\\c:/", "");
    CHECK_SPLIT("//?/c:/", "//?/c:/", "");
    CHECK_SPLIT("//./C:/foo/bin", "//./C:/foo", "bin");
    CHECK_SPLIT("//./C:/bin", "//./C:/", "bin");
    CHECK_SPLIT("\\\\.\\C:\\foo\\bin", "\\\\.\\C:\\foo", "bin");
    CHECK_SPLIT("\\\\.\\C:\\bin", "\\\\.\\C:\\", "bin");
    CHECK_SPLIT("//./C:/", "//./C:/", "");
    CHECK_SPLIT("//./C:", "//./C:", "");
    CHECK_SPLIT("//./", "//./", "");
    CHECK_SPLIT("\\\\.\\", "\\\\.\\", "");
    CHECK_SPLIT("//./foo", "//./foo", "");
    CHECK_SPLIT("//./pipe/name", "//./pipe/", "name");
}

void test_split_unc()
{
    CHECK_SPLIT("//server/share", "//server/share", "");
    CHECK_SPLIT("//server/share/", "//server/share/", "");
    CHECK_SPLIT("//server/share/dir", "//server/share/", "dir");
    CHECK_SPLIT("//server/share/dir/x.exe", "//server/share/dir", "x.exe");
    CHECK_SPLIT("\\\\server\\share\\dir\\x.exe", "\\\\server\\share\\dir", "x.exe");
    CHECK_SPLIT("//server/share//dir//x.exe", "//server/share//dir", "x.exe");
    CHECK_SPLIT("//?/UNC/server/share", "//?/UNC/server/share", "");
    CHECK_SPLIT("//?/UNC/server/share/dir/x.exe", "//?/UNC/server/share/dir", "x.exe");
    CHECK_SPLIT("\\\\?\\UNC\\server\\share\\dir", "\\\\?\\UNC\\server\\share\\", "dir");
    CHECK_SPLIT("//?/unc/server/share/dir", "//?/unc/server/share/", "dir");
    CHECK_SPLIT("\\\\server\\share", "\\\\server\\share", "");
    CHECK_SPLIT("\\\\server\\share\\", "\\\\server\\share\\", "");
    CHECK_SPLIT("\\\\server\\share\\dir", "\\\\server\\share\\", "dir");
    CHECK_SPLIT("\\\\?\\UNC\\server\\share", "\\\\?\\UNC\\server\\share", "");
    CHECK_SPLIT("\\\\?\\UNC\\server\\share\\", "\\\\?\\UNC\\server\\share\\", "");
    CHECK_SPLIT("\\\\?\\UNC\\server\\share\\dir\\x.exe", "\\\\?\\UNC\\server\\share\\dir", "x.exe");
    CHECK_SPLIT("\\\\?\\unc\\server\\share\\", "\\\\?\\unc\\server\\share\\", "");
    CHECK_SPLIT("//?/UNC/server/share/", "//?/UNC/server/share/", "");
    CHECK_SPLIT("//./UNC/server/share", "//./UNC/server/share", "");
    CHECK_SPLIT("//./UNC/server/share/dir", "//./UNC/server/share/", "dir");
}

#else

void test_split_no_windows_roots()
{
    CHECK_SPLIT("C:/foo", "C:", "foo");
    CHECK_SPLIT("C:\\foo", "", "C:\\foo");
    CHECK_SPLIT("foo\\bar", "", "foo\\bar");
    CHECK_SPLIT("//?/C:/foo", "//?/C:", "foo");
    CHECK_SPLIT("//./C:/foo", "//./C:", "foo");
    CHECK_SPLIT("//server/share/dir", "//server/share", "dir");
}

#endif

void test_extended_length_path()
{
    CHECK_EXTENDED("D:/foo/bin/x.exe", "//?/D:/foo/bin/x.exe");
    CHECK_EXTENDED("//?/D:/foo/bin/x.exe", "//?/D:/foo/bin/x.exe");
    CHECK_EXTENDED("//server/share/foo/bin/x.exe", "//?/UNC/server/share/foo/bin/x.exe");
    CHECK_EXTENDED("//?/UNC/server/share/foo/bin/x.exe", "//?/UNC/server/share/foo/bin/x.exe");
    CHECK_EXTENDED("//./D:/foo/bin/x.exe", "//?/D:/foo/bin/x.exe");
    CHECK_EXTENDED("//./UNC/server/share/foo/bin/x.exe", "//?/UNC/server/share/foo/bin/x.exe");
}

void test_install_prefix()
{
    CHECK_PREFIX("D:/foo/bin/x.exe", "//?/D:/foo");
    CHECK_PREFIX("//?/D:/foo/bin/x.exe", "//?/D:/foo");
    CHECK_PREFIX("//server/share/foo/bin/x.exe", "//?/UNC/server/share/foo");
    CHECK_PREFIX("//?/UNC/server/share/foo/bin/x.exe", "//?/UNC/server/share/foo");
    CHECK_PREFIX("//./D:/foo/bin/x.exe", "//?/D:/foo");
    CHECK_PREFIX("//./UNC/server/share/foo/bin/x.exe", "//?/UNC/server/share/foo");
#ifdef _WIN32
    CHECK_PREFIX("D:/x.exe", "//?/D:/");
    CHECK_PREFIX("//server/share/x.exe", "//?/UNC/server/share/");
#endif
}

void test_split_views()
{
    constexpr auto path = "foo/bar/baz"_s;
    const auto parts = split(path);
    fm_assert(parts.first().data() == path.data());
    fm_assert(parts.second().data() == path.data() + 8);
    fm_assert(parts.second().flags() & StringViewFlag::NullTerminated);
}

void test_current_directory()
{
    const auto expected = Path::currentDirectory();
    fm_assert(expected);
    const auto actual = current_directory();
    fm_assert(!actual.isEmpty());
    fm_assert(actual.data()[actual.size()] == '\0');
    fm_assert_equal(StringView{*expected}, StringView{actual});
}

void test_executable_location()
{
    const auto expected = Path::executableLocation();
    fm_assert(expected);
    const auto actual = executable_location();
    fm_assert(!actual.isEmpty());
    fm_assert(actual.data()[actual.size()] == '\0');
    fm_assert_equal(StringView{*expected}, StringView{actual});
    fm_assert(Path::exists(actual));
}

#undef CHECK_PREFIX
#undef CHECK_EXTENDED
#undef CHECK_SPLIT

} // namespace

void test_path()
{
    test_split_posix();
#ifdef _WIN32
    test_split_drive();
    test_split_extended();
    test_split_unc();
#else
    test_split_no_windows_roots();
#endif
    test_split_views();
    test_extended_length_path();
    test_install_prefix();
    test_current_directory();
    test_executable_location();
}

} // namespace floormat::Test
