#include "app.hpp"
#include "compat/exception.inl"
#include "compat/gl-error.hpp"
#include <sstream>
#include <cr/ScopeGuard.h>
#include <mg/Context.h>
#include <mg/Extensions.h>
#include <mg/Renderer.h>

namespace floormat::Test {

namespace {

void with_gl_errors_throws_only_for_errors_inside_with_error_policy()
{
    const Error silence{nullptr};

    bool caught = false;
    glViewport(0, 0, -1, -1);
    try { const with_gl_errors scope{gl_error_policy::error}; }
    catch (const floormat::exception&) { caught = true; }
    fm_assert(!caught);
    fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);

    try
    {
        const with_gl_errors scope{gl_error_policy::error};
        glViewport(0, 0, -1, -1);
    }
    catch (const floormat::exception&) { caught = true; }
    fm_assert(caught);
    fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);

    caught = false;
    try
    {
        const with_gl_errors scope;
        glViewport(0, 0, -1, -1);
    }
    catch (const floormat::exception&) { caught = true; }
    fm_assert(!caught);
    fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);
}

void fm_check_glcall_throws_only_for_errors_from_its_call()
{
    const Error silence{nullptr};

    bool caught = false;
    glViewport(0, 0, -1, -1);
    try { fm_assert(fm_check_glcall(glFlush())); }
    catch (const floormat::exception&) { caught = true; }
    fm_assert(!caught);
    fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);

    try { (void)fm_check_glcall(glViewport(0, 0, -1, -1)); }
    catch (const floormat::exception& e)
    {
        caught = true;
        const auto what = StringView{e.what()};
        fm_assert(what.contains("from glViewport(0, 0, -1, -1) in "_s));
        fm_assert(what.contains(" in "_s + StringView{std::source_location::current().function_name()} + " on "_s));
    }
    fm_assert(caught);
    fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);
}

void fm_warn_glcall_returns_false_only_for_errors_from_its_call()
{
    std::ostringstream out;
    {
        const Error redirect{&out};
        glViewport(0, 0, -1, -1);
        fm_assert(fm_warn_glcall(glFlush()));
        fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);

        fm_assert(!fm_warn_glcall(glViewport(0, 0, -1, -1)));
        fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);
    }
    const auto str = out.str();
    const auto text = StringView{str.data(), str.size()};
    fm_assert(text.contains("from glViewport(0, 0, -1, -1) in "_s));
    fm_assert(text.contains(" in "_s + StringView{std::source_location::current().function_name()} + " on "_s));
}

void fm_warn_glcall_returns_result_and_false_on_error()
{
    const Error silence{nullptr};

    int i = 0;
    const auto ref = fm_warn_glcall(i = 1);
    static_assert(std::is_same_v<decltype(ref), const Pair<Reference<int>, bool>>);
    fm_assert(&ref.first().get() == &i && ref.second());

    const auto value = fm_warn_glcall(i + 1);
    static_assert(std::is_same_v<decltype(value), const Pair<int, bool>>);
    fm_assert(value.first() == 2 && value.second());

    const auto ok = fm_warn_glcall(glFlush());
    static_assert(std::is_same_v<decltype(ok), const bool>);
    fm_assert(ok);

    const auto with_error = fm_warn_glcall(glViewport(0, 0, -1, -1), i);
    fm_assert(&with_error.first().get() == &i && !with_error.second());
}

void ignore_policy_discards_errors_without_printing()
{
    std::ostringstream out;
    {
        const Error redirect{&out};
        glViewport(0, 0, -1, -1);
        fm_assert(fm_glcall(glFlush()));
        fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);

        fm_assert(!fm_glcall(glViewport(0, 0, -1, -1)));
        fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);

        {
            const with_gl_errors scope{gl_error_policy::ignore};
            glViewport(0, 0, -1, -1);
        }
        fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);
    }
    fm_assert(out.str().empty());
}

void inner_scopes_leave_earlier_errors_to_enclosing_scope()
{
    const Error silence{nullptr};

    bool caught = false;
    try
    {
        const with_gl_errors scope{gl_error_policy::error};
        glViewport(0, 0, -1, -1);
        fm_assert(fm_warn_glcall(glFlush()));
        fm_assert(fm_glcall(glFlush()));
        { const with_gl_errors inner{gl_error_policy::ignore}; }
    }
    catch (const floormat::exception&) { caught = true; }
    fm_assert(caught);
    fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);

    caught = false;
    try
    {
        const with_gl_errors scope{gl_error_policy::error};
        fm_assert(!fm_warn_glcall(glViewport(0, 0, -1, -1)));
        {
            const with_gl_errors inner{gl_error_policy::ignore};
            glViewport(0, 0, -1, -1);
        }
    }
    catch (const floormat::exception&) { caught = true; }
    fm_assert(!caught);
    fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);

    std::ostringstream out;
    try
    {
        const Error redirect{&out};
        const with_gl_errors scope;
        glViewport(0, 0, -1, -1);
        fm_assert(fm_check_glcall(glFlush()));
    }
    catch (const floormat::exception&) { caught = true; }
    fm_assert(!caught);
    fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);
    const auto str = out.str();
    const auto text = StringView{str.data(), str.size()};
    fm_assert(text.contains("InvalidValue in "_s + StringView{std::source_location::current().function_name()}));
}

void scope_closed_by_exception_reports_without_throwing()
{
    std::ostringstream out;
    bool caught = false;
    {
        const Error redirect{&out};
        try
        {
            const with_gl_errors scope{gl_error_policy::error};
            glViewport(0, 0, -1, -1);
            (void)fm_check_glcall(glViewport(0, 0, -1, -1));
        }
        catch (const floormat::exception& e)
        {
            caught = true;
            fm_assert(StringView{e.what()}.contains("from glViewport(0, 0, -1, -1) in "_s));
        }
    }
    fm_assert(caught);
    fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);
    const auto str = out.str();
    const auto text = StringView{str.data(), str.size()};
    fm_assert(text.contains("InvalidValue in "_s + StringView{std::source_location::current().function_name()}));
}

void check_invalid_viewport()
{
    (void)fm_check_glcall(glViewport(0, 0, -1, -1));
}

void scope_under_ignore_policy_reports_without_throwing()
{
    std::ostringstream out;
    bool caught = false;
    {
        const Error redirect{&out};
        try
        {
            const with_gl_errors scope{gl_error_policy::ignore};
            fm_assert(!fm_check_glcall(glViewport(0, 0, -1, -1)));
            {
                const with_gl_errors inner{gl_error_policy::error};
                glViewport(0, 0, -1, -1);
            }
            check_invalid_viewport();
        }
        catch (const floormat::exception&) { caught = true; }
        fm_assert(!caught);

        try { check_invalid_viewport(); }
        catch (const floormat::exception&) { caught = true; }
        fm_assert(caught);
    }
    fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);
    const auto str = out.str();
    const auto text = StringView{str.data(), str.size()};
    fm_assert(text.contains("InvalidValue from glViewport(0, 0, -1, -1) in "_s));
    fm_assert(text.contains("InvalidValue in "_s + StringView{std::source_location::current().function_name()}));
    fm_assert(text.contains("check_invalid_viewport"_s));
}

void APIENTRY count_debug_error_messages(GLenum, GLenum type, GLuint, GLenum, GLsizei, const GLchar*,
                                         const void* user_param)
{
    if (type == GL_DEBUG_TYPE_ERROR)
        ++*static_cast<u32*>(const_cast<void*>(user_param));
}

void ignore_policy_sends_no_debug_message_to_callback()
{
    if (!GL::Context::current().isExtensionSupported<GL::Extensions::KHR::debug>())
        return;

    void* callback = nullptr;
    void* user_param = nullptr;
    glGetPointerv(GL_DEBUG_CALLBACK_FUNCTION, &callback);
    glGetPointerv(GL_DEBUG_CALLBACK_USER_PARAM, &user_param);
    const bool is_output_enabled = glIsEnabled(GL_DEBUG_OUTPUT);
    const bool is_output_synchronous = glIsEnabled(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    const auto debug_group_stack_depth = [] {
        GLint depth = 0;
        glGetIntegerv(GL_DEBUG_GROUP_STACK_DEPTH, &depth);
        return depth;
    };
    const GLint initial_debug_group_stack_depth = debug_group_stack_depth();

    u32 num_error_messages = 0;
    glDebugMessageCallback(count_debug_error_messages, &num_error_messages);
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, // earlier tests may have disabled these
                          GL_DONT_CARE, 0, nullptr, GL_TRUE);

    glViewport(0, 0, -1, -1);
    while (GL::Renderer::error() != GL::Renderer::Error::NoError)
        continue;
    const bool is_error_sent_to_callback = num_error_messages > 0;
    num_error_messages = 0;

    fm_assert(!fm_glcall(glViewport(0, 0, -1, -1)));
    {
        const with_gl_errors scope{gl_error_policy::ignore};
        glViewport(0, 0, -1, -1);
        fm_assert(!fm_glcall(glViewport(0, 0, -1, -1)));
        glViewport(0, 0, -1, -1);
    }
    fm_assert(GL::Renderer::error() == GL::Renderer::Error::NoError);
    fm_assert(!is_error_sent_to_callback || num_error_messages == 0);

    std::ostringstream out;
    {
        const Error redirect{&out};
        const with_gl_errors scope{gl_error_policy::ignore};
        fm_assert(!fm_warn_glcall(glViewport(0, 0, -1, -1)));
    }
    const auto str = out.str();
    fm_assert(StringView{str.data(), str.size()}.contains("InvalidValue from glViewport(0, 0, -1, -1) in "_s));

    glViewport(0, 0, -1, -1);
    while (GL::Renderer::error() != GL::Renderer::Error::NoError)
        continue;
    fm_assert(!is_error_sent_to_callback || num_error_messages > 0);

    num_error_messages = 0;
    glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, 0, nullptr, GL_FALSE);
    fm_assert(!fm_glcall(glViewport(0, 0, -1, -1)));
    glViewport(0, 0, -1, -1);
    while (GL::Renderer::error() != GL::Renderer::Error::NoError)
        continue;
    fm_assert(!is_error_sent_to_callback || num_error_messages == 0);
    glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, 0, nullptr, GL_TRUE);

    fm_assert(debug_group_stack_depth() == initial_debug_group_stack_depth);

    glDebugMessageCallback(reinterpret_cast<GLDEBUGPROC>(callback), user_param);
    if (!is_output_synchronous)
        glDisable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    if (!is_output_enabled)
        glDisable(GL_DEBUG_OUTPUT);
}

} // namespace

void test_gl_error()
{
    ScopeGuard debug_output_guard{NoCreate};
    if (GL::Context::current().isExtensionSupported<GL::Extensions::KHR::debug>() && glIsEnabled(GL_DEBUG_OUTPUT))
    {
        GL::Renderer::disable(GL::Renderer::Feature::DebugOutput);
        debug_output_guard = ScopeGuard{[] { GL::Renderer::enable(GL::Renderer::Feature::DebugOutput); }};
    }

    with_gl_errors_throws_only_for_errors_inside_with_error_policy();
    fm_check_glcall_throws_only_for_errors_from_its_call();
    fm_warn_glcall_returns_false_only_for_errors_from_its_call();
    fm_warn_glcall_returns_result_and_false_on_error();
    ignore_policy_discards_errors_without_printing();
    inner_scopes_leave_earlier_errors_to_enclosing_scope();
    scope_closed_by_exception_reports_without_throwing();
    scope_under_ignore_policy_reports_without_throwing();
    ignore_policy_sends_no_debug_message_to_callback();
}

} // namespace floormat::Test
