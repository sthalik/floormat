#include "atlas.hpp"
#include "loader.hpp"
#include "tile-shader.hpp"

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

#include <Magnum/GlmIntegration/Integration.h>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

namespace Magnum::Examples {

struct application final : Platform::Application
{
    using dpi_policy = Platform::Implementation::Sdl2DpiScalingPolicy;

    explicit application(const Arguments& arguments);
    virtual ~application();
    void drawEvent() override;

    GL::Mesh _mesh;
    tile_shader _shader;
    std::shared_ptr<atlas_texture> atlas =
        //loader.tile_atlas("../share/game/images/tiles.tga", {8,4});
        //loader.tile_atlas("../share/game/images/tiles2.tga", {8,5});
        //loader.tile_atlas("../share/game/images/metal1.tga", {2, 2});
        loader.tile_atlas("../share/game/images/floor1.tga", {4, 4});

    static glm::mat<4, 4, double> make_projection(Vector2i window_size, Vector3 offset);
    static float projection_size_ratio();
    Matrix4x4 make_projection(Vector3 offset) const;
};

float application::projection_size_ratio()
{
    auto m = make_projection({1, 1}, {});
    glm::vec<4, double> pos = glm::vec<4, double>{.5, 0, 0, 1} * m;
    return (float)(pos[0] / pos[3]);
}

glm::mat<4, 4, double> application::make_projection(Vector2i window_size, Vector3 offset)
{
    using vec3 = glm::vec<3, double>;
    using mat4 = glm::mat<4, 4, double>;
    double x = window_size[0]*.5, y = window_size[1]*.5, w = 2*std::sqrt(x*x+y*y);
    auto m = glm::ortho(-x, x, -y, y, -w, w);
    //m = glm::ortho<double>(-.5, .5, -.5, .5, -100, 100);
    m = glm::scale(m, { 1., 0.6, 1. });
    m = glm::translate(m, { (double)offset[0], (double)-offset[1], (double)offset[2] });
    m = glm::rotate(m, glm::radians(std::asin(1./std::sqrt(2))), vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(-45.), vec3(0, 0, 1));

    return glm::mat4(m);
}

Matrix4x4 application::make_projection(Vector3 offset) const
{
    return Magnum::Matrix4x4{glm::mat4{make_projection(windowSize(), offset)}};
}

application::application(const Arguments& arguments):
    Platform::Application{arguments, Configuration{}
        .setTitle("Test")
        .setSize({1024, 768}, dpi_policy::Physical)}
{
    struct QuadVertex {
        Vector3 position;
        Vector2 textureCoordinates;
        // todo gl_FragDepth
    };

    std::vector<QuadVertex> vertices; vertices.reserve(64*64*4);
    std::vector<UnsignedShort> indices; indices.reserve(256);

    auto sz = Vector2{50, 50} * projection_size_ratio();

    int k = 0;
    for (int j = -2; j <= 2; j++)
        for (int i = -2; i <= 2; i++)
        {
            auto positions = atlas->floor_quad({(float)(sz[0]*i), (float)(sz[1]*j), 0}, sz);
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

void application::drawEvent() {
    GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

    //GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    //GL::Renderer::setDepthMask(true);

    using namespace Math::Literals;

    _shader
        .set_projection(make_projection({}))
        .set_color(0xffffff_rgbf)
        .bindTexture(atlas->texture())
        .draw(_mesh);

    swapBuffers();
}

application::~application()
{
    loader_::destroy();
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
