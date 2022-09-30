#include "tile-atlas.hpp"
#include "loader.hpp"
#include "tile-shader.hpp"
#include "defs.hpp"
#include "tile.hpp"

#include <bitset>

#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/PluginManager/Manager.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector.h>
#include <Magnum/ImageView.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/Trade/AbstractImporter.h>

#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <Magnum/GlmIntegration/Integration.h>
#include <SDL_timer.h>

namespace Magnum::Examples {

template<typename enum_type>
struct enum_bitset : std::bitset<(std::size_t)enum_type::MAX> {
    static_assert(std::is_same_v<std::size_t, std::common_type_t<std::size_t, std::underlying_type_t<enum_type>>>);
    static_assert(std::is_same_v<enum_type, std::decay_t<enum_type>>);
    using std::bitset<(std::size_t)enum_type::MAX>::bitset;
    constexpr bool operator[](enum_type x) const { return operator[]((std::size_t)x); }
    constexpr decltype(auto) operator[](enum_type x) {
        return std::bitset<(std::size_t)enum_type::MAX>::operator[]((std::size_t)x);
    }
};

struct app final : Platform::Application
{
    using dpi_policy = Platform::Implementation::Sdl2DpiScalingPolicy;

    explicit app(const Arguments& arguments);
    virtual ~app();
    void drawEvent() override;
    void update(float dt);
    void do_camera(float dt);
    void reset_camera_offset();
    void keyPressEvent(KeyEvent& event) override;
    void keyReleaseEvent(KeyEvent& event) override;
    void do_key(KeyEvent::Key k, KeyEvent::Modifiers m, bool pressed, bool repeated);

    enum class key : int {
        camera_up, camera_left, camera_right, camera_down, camera_reset,
        quit,
        MAX
    };

    GL::Mesh _mesh, _mesh2;
    tile_shader _shader;
    std::shared_ptr<tile_atlas> atlas =
        //loader.tile_atlas("../share/game/images/tiles.tga", {8,4});
        //loader.tile_atlas("../share/game/images/tiles2.tga", {8,5});
        loader.tile_atlas("../share/game/images/metal1.tga", {2, 2});
        //loader.tile_atlas("../share/game/images/floor1.tga", {4, 4});
    std::shared_ptr<tile_atlas> atlas2 =
        loader.tile_atlas("../share/game/images/metal2.tga", {2, 2});

    std::uint64_t time_ticks = 0, time_freq = SDL_GetPerformanceFrequency();
    Vector2 camera_offset;
    enum_bitset<key> keys;

    float get_dt();
    static constexpr Vector3 TILE_SIZE = { 50, 50, 50 };
};

using namespace Math::Literals;

app::app(const Arguments& arguments):
    Platform::Application{
          arguments,
          Configuration{}
              .setTitle("Test")
              .setSize({1024, 768}, dpi_policy::Physical),
          GLConfiguration{}
              //.setSampleCount(4)
    }
{
    struct QuadVertex {
        Vector3 position;
        Vector2 textureCoordinates;
    };

    std::vector<QuadVertex> vertices; vertices.reserve(1024);
    std::vector<UnsignedShort> indices; indices.reserve(1024);

    //float ratio = projection_size_ratio();
    const float X = TILE_SIZE[0], Y = TILE_SIZE[1], Z = TILE_SIZE[2];

    reset_camera_offset();

    {
        vertices.clear();
        indices.clear();
        int k = 0;
        for (unsigned j = 0; j < chunk::N; j++) // TODO draw walls in correct order
            for (unsigned i = 0; i < chunk::N; i++)
            {
                auto positions = atlas->floor_quad({(float)(X*i), (float)(Y*j), 0}, {X, Y});
                auto texcoords = atlas->texcoords_for_id(k % atlas->size());
                auto indices_  = atlas->indices(k);

                for (unsigned x = 0; x < 4; x++)
                    vertices.push_back({ positions[x], texcoords[x] });
                for (auto x : indices_)
                    indices.push_back(x);
                k++;
            }

        _mesh.setCount((int)indices.size())
            .addVertexBuffer(GL::Buffer{vertices}, 0,
                             tile_shader::Position{}, tile_shader::TextureCoordinates{})
            .setIndexBuffer(GL::Buffer{indices}, 0, GL::MeshIndexType::UnsignedShort);
    }

    vertices.clear();
    indices.clear();

    {
        Vector3 center{chunk::N/2.f*TILE_SIZE[0], chunk::N/2.f*TILE_SIZE[1], 0};
        tile_atlas::vertex_array_type walls[] = {
            atlas2->wall_quad_W(center, Vector3(X, Y, Z)),
            atlas2->wall_quad_N(center, Vector3(X, Y, Z)),
            atlas2->wall_quad_E(center, Vector3(X, Y, Z)),
            atlas2->wall_quad_S(center, Vector3(X, Y, Z)),
        };

        int k = 0;
        for (const auto& positions : walls)
        {
            auto texcoords = atlas2->texcoords_for_id(k % atlas2->size());
            auto indices_ = atlas2->indices(k);
            for (unsigned x = 0; x < 4; x++)
                vertices.push_back({ positions[x], texcoords[x] });
            for (auto x : indices_)
                indices.push_back(x);
            k++;
        }

        //auto positions = anim_atlas->floor_quad({(float)(sz[0]*0), (float)(sz[1]*0), sz[1]*2}, sz);
    }

    _mesh2.setCount((int)indices.size())
        .addVertexBuffer(GL::Buffer{vertices}, 0,
                         tile_shader::Position{}, tile_shader::TextureCoordinates{})
        .setIndexBuffer(GL::Buffer{indices}, 0, GL::MeshIndexType::UnsignedShort);

    (void)get_dt();
}

void app::drawEvent() {
    GL::defaultFramebuffer.clear(GL::FramebufferClear::Color);

    //GL::defaultFramebuffer.clear(GL::FramebufferClear::Depth);
    //GL::Renderer::setDepthMask(true);
    //GL::Renderer::setDepthFunction(GL::Renderer::DepthFunction::LessOrEqual);
    //GL::Renderer::enable(GL::Renderer::Feature::DepthTest);

    {
        float dt = get_dt();
        update(dt);
    }

    {
        //auto ratio = projection_size_ratio();
        auto sz = windowSize();
        _shader.set_scale({ (float)sz[0], (float)sz[1] });
        static bool once = true;
        if (once) {
            once = false;
            Debug{} << _shader.project({16*50, 0, 0});
        }
    }

#if 1
    _shader
        .bindTexture(atlas->texture())
        .draw(_mesh);
#endif
#if 1
    _shader
        .bindTexture(atlas2->texture())
        .draw(_mesh2);
#endif

    swapBuffers();
    redraw();
}

void app::do_camera(float dt)
{
    constexpr float pixels_per_second = 100;
    if (keys[key::camera_up])
        camera_offset += Vector2(0, 1) * dt * pixels_per_second;
    else if (keys[key::camera_down])
        camera_offset += Vector2(0, -1)  * dt * pixels_per_second;
    if (keys[key::camera_left])
        camera_offset += Vector2(1, 0) * dt * pixels_per_second;
    else if (keys[key::camera_right])
        camera_offset += Vector2(-1, 0)  * dt * pixels_per_second;

    _shader.set_camera_offset(camera_offset);

    if (keys[key::camera_reset])
        reset_camera_offset();
}

void app::reset_camera_offset()
{
    camera_offset = _shader.project({chunk::N*TILE_SIZE[0]/2.f, chunk::N*TILE_SIZE[1]/2.f, 0});
}

void app::update(float dt)
{
    do_camera(dt);
    if (keys[key::quit])
        Platform::Sdl2Application::exit(0);
}

void app::do_key(KeyEvent::Key k, KeyEvent::Modifiers m, bool pressed, bool repeated)
{
    //using Mods = KeyEvent::Modifiers;

    (void)m;
    (void)repeated;

    const key x = progn(switch (k) {
        using enum KeyEvent::Key;
        using enum key;

        case W:     return camera_up;
        case A:     return camera_left;
        case S:     return camera_down;
        case D:     return camera_right;
        case Home:  return camera_reset;
        case Esc:   return quit;
        default:    return MAX;
    });

    if (x != key::MAX)
        keys[x] = pressed;
}

float app::get_dt()
{
    const std::uint64_t t = SDL_GetPerformanceCounter();
    float dt = (float)((t - time_ticks) / (double)time_freq);
    time_ticks = t;
    return dt;
}

app::~app()
{
    loader_::destroy();
}

void app::keyPressEvent(Platform::Sdl2Application::KeyEvent& event)
{
    do_key(event.key(), event.modifiers(), true, event.isRepeated());
}

void app::keyReleaseEvent(Platform::Sdl2Application::KeyEvent& event)
{
    do_key(event.key(), event.modifiers(), false, false);
}

} // namespace Magnum::Examples

MAGNUM_APPLICATION_MAIN(Magnum::Examples::app);

#ifdef _MSC_VER
#   include <cstdlib>
#   ifdef __clang__
#       pragma clang diagnostic ignored "-Wmissing-prototypes"
#       pragma clang diagnostic ignored "-Wmain"
#   endif

extern "C" int __stdcall WinMain(void*, void*, void*, int /* nCmdShow */) {
    return main(__argc, __argv);
}
#endif
