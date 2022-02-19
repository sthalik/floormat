#include "atlas.hpp"
#include "tile-shader.hpp"

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/Utility/Resource.h>
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
#include <Magnum/Trade/ImageData.h>

#include <Magnum/GlmIntegration/Integration.h>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

namespace Magnum::Examples {

struct application final : Platform::Application
{
    explicit application(const Arguments& arguments);
    void drawEvent() override;

    const Utility::Resource rs{"texturedquad-data"};
    PluginManager::Manager<Trade::AbstractImporter> plugins;
    Containers::Pointer<Trade::AbstractImporter> tga_importer =
        plugins.loadAndInstantiate("TgaImporter");

    GL::Mesh _mesh;
    tile_shader _shader;
    atlas_texture atlas = make_atlas("../share/game/images/tiles.tga", {8, 4});

    atlas_texture make_atlas(const std::string& file, Vector2i dims);
    Matrix4x4 make_projection(Vector3 offset);
};

using dpi_policy = Platform::Implementation::Sdl2DpiScalingPolicy;

application::application(const Arguments& arguments):
    Platform::Application{arguments, Configuration{}
        .setTitle("Test")
        .setSize({640, 480}, dpi_policy::Physical)}
{
    struct QuadVertex {
        Vector3 position;
        Vector2 textureCoordinates;
        // todo gl_FragDepth
    };

    std::vector<QuadVertex> vertices; vertices.reserve(64*64*4);
    std::vector<Short> indices; indices.reserve(64*64*4);

    int k = 0;
    for (int j = -2; j <= 2; j++)
        for (int i = -2; i <= 2; i++)
        {
            constexpr int sz = 48;
            auto positions = atlas.floor_quad({(float)(sz*i), (float)(sz*j), 0}, {sz, sz});
            auto texcoords = atlas.texcoords_for_id(k % atlas.size());
            auto indices_  = atlas.indices(k);

            for (unsigned x = 0; x < 4; x++)
                vertices.push_back({ positions[x], texcoords[x] });
            for (auto x : indices_)
                indices.push_back((Short)x);
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
        .set_projection(make_projection({0, 0, 0}))
        .set_color(0xffffff_rgbf)
        .bindTexture(atlas.texture())
        .draw(_mesh);

    swapBuffers();
}

atlas_texture application::make_atlas(const std::string& file, Vector2i dims)
{
    if(!tga_importer || !tga_importer->openFile(file))
        std::exit(1);

    Containers::Optional<Trade::ImageData2D> image = tga_importer->image2D(0);
    CORRADE_INTERNAL_ASSERT(image);

    return atlas_texture{*image, dims};
}

Matrix4x4 application::make_projection(Vector3 offset)
{
    using vec3 = glm::vec<3, double, glm::highp>;
    using mat4 = glm::mat<4, 4, double, glm::highp>;
    auto m = mat4{1};
    auto size = windowSize();
    double x = size[0]*.5, y = size[1]*.5, w = 4*sqrt(x*x+y*y);
    m = glm::ortho<double>(-x, x, -y, y, -w, w);
    m = glm::translate(m, { (double)offset[0], (double)-offset[1], (double)offset[2] });
    m = glm::scale(m, { 1., 0.6, 1. });
    m = glm::rotate(m, glm::radians(-45.), vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(0.),   vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(-45.), vec3(0, 0, 1));
    return Matrix4x4{glm::mat4(m)};
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
