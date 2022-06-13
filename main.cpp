#include "atlas.hpp"
#include "loader.hpp"
#include "tile-shader.hpp"
#include "defs.hpp"

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

struct application final : Platform::Application
{
    using dpi_policy = Platform::Implementation::Sdl2DpiScalingPolicy;

    explicit application(const Arguments& arguments);
    virtual ~application();
    void drawEvent() override;
    void update(float dt);
    void keyPressEvent(KeyEvent& event) override;
    void keyReleaseEvent(KeyEvent& event) override;
    void do_key(KeyEvent::Key k, KeyEvent::Modifiers m, bool pressed, bool repeated);

    enum class key {
        camera_up, camera_left, camera_right, camera_down, camera_reset,
        MAX
    };

    GL::Mesh _mesh, _mesh2;
    tile_shader _shader;
    std::shared_ptr<atlas_texture> atlas =
        //loader.tile_atlas("../share/game/images/tiles.tga", {8,4});
        //loader.tile_atlas("../share/game/images/tiles2.tga", {8,5});
        loader.tile_atlas("../share/game/images/metal1.tga", {2, 2});
        //loader.tile_atlas("../share/game/images/floor1.tga", {4, 4});
    std::shared_ptr<atlas_texture> atlas2 =
        loader.tile_atlas("../share/game/images/metal2.tga", {2, 2});

    std::uint64_t time_ticks = 0, time_freq = SDL_GetPerformanceFrequency();
    Vector3 camera_offset;
    std::bitset<(std::size_t)key::MAX> keys{0ul};

    float get_dt();
    static const Vector3 TILE_SIZE;
};

const Vector3 application::TILE_SIZE = { 50, 50, 50 };

using namespace Math::Literals;

application::application(const Arguments& arguments):
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

    {
        vertices.clear();
        indices.clear();
        int k = 0;
        for (int j = -2; j <= 2; j++)
            for (int i = -2; i <= 2; i++)
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
        atlas_texture::vertex_array_type walls[] = {
            atlas2->wall_quad_W({}, Vector3(X, Y, Z)),
            atlas2->wall_quad_N({}, Vector3(X, Y, Z)),
            atlas2->wall_quad_E({}, Vector3(X, Y, Z)),
            atlas2->wall_quad_S({}, Vector3(X, Y, Z)),
        };

        int k = 0;
        for (const auto& positions : walls)
        {
            auto texcoords = atlas2->texcoords_for_id(k);
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

void application::drawEvent() {
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
    //redraw();
}

void application::update(float dt)
{
    constexpr float pixels_per_second = 100;
    if (keys[(int)key::camera_up])
        camera_offset += Vector3(0, -1, 0)  * dt * pixels_per_second;
    else if (keys[(int)key::camera_down])
        camera_offset += Vector3(0, 1, 0) * dt * pixels_per_second;
    if (keys[(int)key::camera_left])
        camera_offset += Vector3(-1, 0, 0) * dt * pixels_per_second;
    else if (keys[(int)key::camera_right])
        camera_offset += Vector3(1, 0, 0)  * dt * pixels_per_second;

    if (keys[(int)key::camera_reset])
        camera_offset = {};
}

void application::do_key(KeyEvent::Key k, KeyEvent::Modifiers m, bool pressed, bool repeated)
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
        default:    return MAX;
    });

    if (x != key::MAX)
        keys[(std::size_t)x] = pressed;
}

float application::get_dt()
{
    const std::uint64_t t = SDL_GetPerformanceCounter();
    float dt = (float)((t - time_ticks) / (double)time_freq);
    time_ticks = t;
    return dt;
}

application::~application()
{
    loader_::destroy();
}

void application::keyPressEvent(Platform::Sdl2Application::KeyEvent& event)
{
    do_key(event.key(), event.modifiers(), true, event.isRepeated());
}

void application::keyReleaseEvent(Platform::Sdl2Application::KeyEvent& event)
{
    do_key(event.key(), event.modifiers(), false, false);
}

} // namespace Magnum::Examples

MAGNUM_APPLICATION_MAIN(Magnum::Examples::application);

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
