#include "main-impl.hpp"
#include "src/search.hpp"
#include "src/search-astar.hpp"
#include <Magnum/Platform/Sdl2Application.h>

namespace floormat {

floormat_main::floormat_main() noexcept = default;
floormat_main::~floormat_main() noexcept = default;

floormat_main* floormat_main::create(floormat_app& app, fm_settings&& options)
{
    auto* ret = new main_impl(app, move(options), options.argc, const_cast<char**>(options.argv));
    return ret;
}

int main_impl::exec()
{
    _framebuffer_size = framebufferSize();
    recalc_viewport(_framebuffer_size, windowSize());
    return Sdl2Application::exec();
}

main_impl::~main_impl() noexcept
{
    reset_world();
}

void main_impl::set_cursor(uint32_t cursor) noexcept
{
    if (cursor != _mouse_cursor || _mouse_cursor == (uint32_t)-1)
    {
        _mouse_cursor = cursor;
        setCursor(Cursor(cursor));
    }
}

uint32_t main_impl::cursor() const noexcept
{
    using App = Platform::Sdl2Application;
    return (uint32_t)static_cast<App*>(const_cast<main_impl*>(this))->cursor();
}

void main_impl::quit(int status) { Platform::Sdl2Application::exit(status); }
class world& main_impl::world() noexcept { return _world; }
SDL_Window* main_impl::window() noexcept { return Sdl2Application::window(); }
fm_settings& main_impl::settings() noexcept { return s; }
const fm_settings& main_impl::settings() const noexcept { return s; }
tile_shader& main_impl::shader() noexcept { return _shader; }
const tile_shader& main_impl::shader() const noexcept { return _shader; }
struct lightmap_shader& main_impl::lightmap_shader() noexcept { return _lightmap_shader; }
bool main_impl::is_text_input_active() const noexcept { return const_cast<main_impl&>(*this).isTextInputActive(); }
void main_impl::start_text_input() noexcept { startTextInput(); }
void main_impl::stop_text_input() noexcept { stopTextInput(); }
Platform::Sdl2Application& main_impl::application() noexcept { return *this; }
const Platform::Sdl2Application& main_impl::application() const noexcept { return *this; }
struct texture_unit_cache& main_impl::texture_unit_cache() { return _tuc; }
path_search& main_impl::search() { return *_search; }
astar& main_impl::astar() { return *_astar; }

Vector2i floormat_main::window_size() const noexcept { return _framebuffer_size; }
void floormat_main::set_render_vobjs(bool value) { _do_render_vobjs = value; }
bool floormat_main::is_rendering_vobjs() const { return _do_render_vobjs; }

float floormat_main::smoothed_frame_time() const noexcept { return _smoothed_frame_time; }

} // namespace floormat
