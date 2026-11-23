#include "app.hpp"
#include "compat/gl-error.hpp"
#include <initializer_list>
#include <mg/Renderer.h>

namespace floormat::Test {

namespace {

void fm_glcall_returns_false_only_for_errors_from_its_call()
{
    const Error silence{nullptr};

    glViewport(0, 0, -1, -1);
    const auto ok = fm_glcall(glFlush());
    static_assert(std::is_same_v<decltype(ok), const bool>);
    fm_assert(ok);
    fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);

    fm_assert(!fm_glcall(glViewport(0, 0, -1, -1)));
    fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);

    fm_assert(fm_glcall(glFlush()));
}

void fm_glcall_preserves_bool_result_separately_from_gl_success()
{
    const Error silence{nullptr};

    for (const bool value : {false, true})
    {
        const auto success = fm_glcall(glFlush(), bool(value));
        static_assert(std::is_same_v<decltype(success), const Pair<bool, bool>>);
        fm_assert(success.first() == value && success.second());

        const auto failure = fm_glcall(glViewport(0, 0, -1, -1), bool(value));
        static_assert(std::is_same_v<decltype(failure), const Pair<bool, bool>>);
        fm_assert(failure.first() == value && !failure.second());
        fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);
    }
}

void fm_glcall_preserves_values_and_references_on_error()
{
    const Error silence{nullptr};

    int i = 17;
    const auto ref = fm_glcall(i += 2);
    static_assert(std::is_same_v<decltype(ref), const Pair<Reference<int>, bool>>);
    fm_assert(&ref.first().get() == &i && ref.second());
    fm_assert(i == 19);

    const auto value = fm_glcall(i + 10);
    static_assert(std::is_same_v<decltype(value), const Pair<int, bool>>);
    fm_assert(value.first() == 29 && value.second());

    const auto ref_with_error = fm_glcall(glViewport(0, 0, -1, -1), i += 4);
    static_assert(std::is_same_v<decltype(ref_with_error), const Pair<Reference<int>, bool>>);
    fm_assert(&ref_with_error.first().get() == &i && !ref_with_error.second());
    fm_assert(i == 23);
    fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);

    const auto value_with_error = fm_glcall(glViewport(0, 0, -1, -1), i + 8);
    static_assert(std::is_same_v<decltype(value_with_error), const Pair<int, bool>>);
    fm_assert(value_with_error.first() == 31 && !value_with_error.second());
    fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);
}

} // namespace

void test_gl_call()
{
    const auto debug_output_guard = silence_gl_debug_output();

    fm_glcall_returns_false_only_for_errors_from_its_call();
    fm_glcall_preserves_bool_result_separately_from_gl_success();
    fm_glcall_preserves_values_and_references_on_error();
}

} // namespace floormat::Test
