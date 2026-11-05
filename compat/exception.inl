#pragma once
#include "compat/exception.hpp"
#include "compat/fwd/exception.hpp"
#include <cr/String.h>

namespace floormat {

struct exception : std::exception, base_exception
{
    exception(exception&&) noexcept;
    exception(const exception& other) noexcept;
    exception& operator=(exception&&) noexcept;
    exception& operator=(const exception& other) noexcept;
    const char* what() const noexcept override;

private:
    friend void detail::vthrow_fmt(fmt::string_view str, fmt::format_args args) noexcept(false);

    explicit exception(Corrade::Containers::String&& str) noexcept;

    Corrade::Containers::String buf;
};

} // namespace floormat
