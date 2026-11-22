#include "gl-error.hpp"
#include "assert.hpp"
#include "exception.hpp"
#include <bit>
#include <exception>
#include <cr/Debug.h>
#include <mg/Context.h>
#include <mg/DebugOutput.h>
#include <mg/Extensions.h>
#include <mg/Functions.h>
#include <mg/Renderer.h>

namespace floormat {

namespace {

using Source = GL::DebugOutput::Source;
using Type   = GL::DebugOutput::Type;

with_gl_errors* innermost_scope = nullptr;
u32 num_open_ignore_policy_scopes = 0;

bool is_gl_error_reported_by_debug_output()
{
    auto& ctx = GL::Context::current();
    void* callback = nullptr;
    if (ctx.isExtensionSupported<GL::Extensions::KHR::debug>() && ctx.flags() & GL::Context::Flag::Debug)
        glGetPointerv(GL_DEBUG_CALLBACK_FUNCTION, &callback);
    return callback && glIsEnabled(GL_DEBUG_OUTPUT) && glIsEnabled(GL_DEBUG_OUTPUT_SYNCHRONOUS);
}

u8 take_gl_errors()
{
    u8 errors = 0;
    for (auto error = GL::Renderer::error(); error != GL::Renderer::Error::NoError; error = GL::Renderer::error())
    {
        const auto bit = (u32)error - GL_INVALID_ENUM;
        fm_assert(bit < 8);
        errors |= u8(1u << bit);
    }
    return errors;
}

ScopeGuard disable_debug_output(std::initializer_list<Pair<Source, Type>> sources_and_types = {})
{
    if (!GL::Context::current().isExtensionSupported<GL::Extensions::KHR::debug>())
        return ScopeGuard{NoCreate};
    else
    {
        constexpr auto debug_group_name = "disable_debug_output"_s;
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0,
                         Math::min((Int)debug_group_name.size(), GL::DebugOutput::maxMessageLength() - 1),
                         debug_group_name.data());
        if (sources_and_types.size() == 0)
            GL::DebugOutput::setEnabled(false);
        else
            for (const auto& source_and_type : sources_and_types)
                GL::DebugOutput::setEnabled(source_and_type.first(), source_and_type.second(), false);
        return ScopeGuard{[] { glPopDebugGroup(); }};
    }
}

} // namespace

with_gl_errors::with_gl_errors(gl_error_policy policy, std::source_location location):
    with_gl_errors{policy, {}, location}
{
}

with_gl_errors::with_gl_errors(gl_error_policy policy, StringView expression, std::source_location location):
    expression{expression}, location{location}, enclosing_scope{innermost_scope}, policy{policy}
{
    const auto errors = take_gl_errors();
    if (enclosing_scope)
        enclosing_scope->error_mask |= errors;
    innermost_scope = this;
    if (policy == gl_error_policy::ignore)
    {
        ++num_open_ignore_policy_scopes;
        debug_output_guard = disable_debug_output({{Source::Api, Type::Error}});
    }
}

with_gl_errors::~with_gl_errors() noexcept(false)
{
    if (is_open)
        (void)close();
}

bool with_gl_errors::close()
{
    fm_debug_assert(innermost_scope == this);
    is_open = false;
    innermost_scope = enclosing_scope;
    if (policy == gl_error_policy::ignore)
        --num_open_ignore_policy_scopes;

    const auto errors = u8(error_mask | take_gl_errors());
    if (!errors)
        return true;
    else if (policy == gl_error_policy::ignore)
        return false;
    else
    {
        const bool is_under_ignore_policy = num_open_ignore_policy_scopes > 0;
        const auto from = expression.isEmpty() ? ""_s : " from "_s;
        if (!is_under_ignore_policy && is_gl_error_reported_by_debug_output())
            ERR_nospace << "GL error" << from << expression << " in " << location.function_name()
                        << " on " << location.file_name() << ":" << location.line();
        else
            for (u32 i = 0; i < 8; i++)
                if (errors & (1u << i))
                    ERR_nospace << "GL error " << GL::Renderer::Error(GL_INVALID_ENUM + i) << from << expression
                                << " in " << location.function_name()
                                << " on " << location.file_name() << ":" << location.line();

        if (policy == gl_error_policy::error && !is_under_ignore_policy && !std::uncaught_exceptions()) [[unlikely]]
            fm_throw("GL error {:#x}{}{} in {} on {}:{}", unsigned(GL_INVALID_ENUM + std::countr_zero(errors)),
                     from, expression, location.function_name(), location.file_name(), location.line());
        return false;
    }
}

} // namespace floormat

namespace floormat::detail {

bool check_glcall(function_view<void()> fn, StringView expression, gl_error_policy policy, std::source_location location)
{
    with_gl_errors scope{policy, expression, location};
    fn();
    return scope.close();
}

} // namespace floormat::detail
