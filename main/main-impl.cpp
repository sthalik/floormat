#include "main-impl.hpp"
#include "compat/assert.hpp"
#include <Magnum/Platform/Sdl2Application.h>

namespace floormat {

floormat_main::floormat_main() noexcept = default;
floormat_main::~floormat_main() noexcept = default;
main_impl::~main_impl() noexcept = default;

void main_impl::quit(int status) { Platform::Sdl2Application::exit(status); }
struct world& main_impl::world() noexcept { return _world; }
SDL_Window* main_impl::window() noexcept { return Sdl2Application::window(); }
fm_settings& main_impl::settings() noexcept { return s; }
const fm_settings& main_impl::settings() const noexcept { return s; }
Vector2i main_impl::window_size() const noexcept { return framebufferSize(); }
tile_shader& main_impl::shader() noexcept { return _shader; }
const tile_shader& main_impl::shader() const noexcept { return _shader; }
bool main_impl::is_text_input_active() const noexcept { return const_cast<main_impl&>(*this).isTextInputActive(); }
void main_impl::start_text_input() noexcept { startTextInput(); }
void main_impl::stop_text_input() noexcept { stopTextInput(); }

int main_impl::exec()
{
    recalc_viewport(framebufferSize(), windowSize());
    return Sdl2Application::exec();
}

floormat_main* floormat_main::create(floormat_app& app, fm_settings&& options)
{
    int fake_argc = 0;
    auto* ret = new main_impl(app, std::move(options), fake_argc);
    fm_assert(ret);
    return ret;
}

void main_impl::set_cursor(std::uint32_t cursor) noexcept
{
    setCursor(Cursor(cursor));
}

std::uint32_t main_impl::cursor() const noexcept
{
    using App = Platform::Sdl2Application;
    return (std::uint32_t)static_cast<App*>(const_cast<main_impl*>(this))->cursor();
}

} // namespace floormat
