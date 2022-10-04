#include "tile-atlas.hpp"
#include "loader.hpp"
#include "shaders/tile-shader.hpp"
#include "tile.hpp"
#include "chunk.hpp"
#include "floor-mesh.hpp"
#include "wall-mesh.hpp"
#include "compat/defs.hpp"

#include <bitset>

#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Timeline.h>

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
    using tile_atlas_ = std::shared_ptr<tile_atlas>;

    explicit app(const Arguments& arguments);
    virtual ~app();
    void drawEvent() override;
    void update(float dt);
    void do_camera(float dt);
    void reset_camera_offset();
    void keyPressEvent(KeyEvent& event) override;
    void keyReleaseEvent(KeyEvent& event) override;
    void do_key(KeyEvent::Key k, KeyEvent::Modifiers m, bool pressed, bool repeated);
    void draw_chunk(chunk& c);
    void update_window_scale();

    enum class key : int {
        camera_up, camera_left, camera_right, camera_down, camera_reset,
        quit,
        MAX
    };
    chunk make_test_chunk();

    tile_shader _shader;
    tile_atlas_ floor1 = loader.tile_atlas("../share/game/images/metal1.tga", {2, 2});
    tile_atlas_ floor2 = loader.tile_atlas("../share/game/images/floor1.tga", {4, 4});
    tile_atlas_ wall1 = loader.tile_atlas("../share/game/images/metal2.tga", {2, 2});
    tile_atlas_ wall2 = loader.tile_atlas("../share/game/images/wood1.tga", {2, 2});
    chunk _chunk = make_test_chunk();
    floor_mesh _floor_mesh;
    wall_mesh _wall_mesh;

    Vector2 camera_offset;
    enum_bitset<key> keys;
    Magnum::Timeline timeline;
};

using namespace Math::Literals;

chunk app::make_test_chunk()
{
    constexpr auto N = TILE_MAX_DIM;
    chunk c;
    c.foreach_tile([&, this](tile& x, std::size_t k, local_coords pt) {
      const auto& atlas = pt.x > N/2 && pt.y >= N/2 ? floor2 : floor1;
      x.ground_image = { atlas, (std::uint8_t)(k % atlas->size()) };
    });
    constexpr auto K = N/2;
    c[{K,   K  }].wall_north = { wall1, 0 };
    c[{K,   K  }].wall_west  = { wall2, 0 };
    c[{K,   K+1}].wall_north = { wall1, 0 };
    c[{K+1, K  }].wall_west  = { wall2, 0 };
    return c;
}

void app::update_window_scale()
{
    auto sz = windowSize();
    _shader.set_scale({ (float)sz[0], (float)sz[1] });
}

void app::draw_chunk(chunk& c)
{
    _floor_mesh.draw(_shader, c);
    _wall_mesh.draw(_shader, c);
}

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
    reset_camera_offset();
#if 0
    std::vector<QuadVertex> vertices; vertices.reserve(1024);
    std::vector<UnsignedShort> indices; indices.reserve(1024);

    //float ratio = projection_size_ratio();
    const float X = TILE_SIZE[0], Y = TILE_SIZE[1], Z = TILE_SIZE[2];

    reset_camera_offset();

    {
        constexpr auto N = TILE_MAX_DIM;
        Vector3 center{N/2.f*TILE_SIZE[0], N/2.f*TILE_SIZE[1], 0};
        tile_atlas::vertex_array_type walls[] = {
            wall1->wall_quad_W(center, Vector3(X, Y, Z)),
            wall1->wall_quad_N(center, Vector3(X, Y, Z)),
            wall1->wall_quad_E(center, Vector3(X, Y, Z)),
            wall1->wall_quad_S(center, Vector3(X, Y, Z)),
        };

        int k = 0;
        for (const auto& positions : walls)
        {
            auto texcoords = wall1->texcoords_for_id(k % wall1->size());
            auto indices_ = wall1->indices(k);
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
#endif

    timeline.start();
}

void app::drawEvent() {
    GL::defaultFramebuffer.clear(GL::FramebufferClear::Color);

#if 1
    GL::defaultFramebuffer.clear(GL::FramebufferClear::Depth);
    GL::Renderer::setDepthMask(true);
    GL::Renderer::setDepthFunction(GL::Renderer::DepthFunction::LessOrEqual);
    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
#endif

    update_window_scale();
    {
        float dt = timeline.previousFrameDuration();
        update(dt);
    }

#if 0
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
#endif

    //auto floor1_sampler = _shader.bind_sampler(floor1);
    //auto wall_sampler = _shader.bind_sampler(wall1);

#if 1
    draw_chunk(_chunk);
#endif

    swapBuffers();
    redraw();
    timeline.nextFrame();
}

void app::do_camera(float dt)
{
    constexpr float pixels_per_second = 512;
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
    camera_offset = _shader.project({TILE_MAX_DIM*TILE_SIZE[0]/2.f, TILE_MAX_DIM*TILE_SIZE[1]/2.f, 0});
    //camera_offset = {};
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

MAGNUM_APPLICATION_MAIN(Magnum::Examples::app)

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
