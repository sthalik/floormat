#include "atlas.hpp"
#include "tile-shader.hpp"

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/Utility/Resource.h>
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
    atlas_texture atlas = make_atlas("images/tiles.tga", {8, 4});

    atlas_texture make_atlas(const std::string& file, Vector2i dims);
    Matrix4x4 make_projection(Vector3 offset);
};

application::application(const Arguments& arguments):
    Platform::Application{arguments, Configuration{}
        .setTitle("Magnum Textured Quad Example")
        .setSize({512, 512})}
{
    struct QuadVertex {
        Vector3 position;
        Vector2 textureCoordinates;
        // todo gl_FragDepth
    };
    QuadVertex vertices[4];
    auto positions = atlas.floor_quad({}, {48, 48});
    auto texcoords = atlas.texcoords_for_id(2);
    auto indices   = atlas.indices(0);

    for (unsigned i = 0; i < std::size(vertices); i++)
        vertices[i] = { positions[i], texcoords[i] };

    _mesh.setCount((int)std::size(indices))
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
    auto m = glm::mat4{1};
    m = glm::ortho<float>(-256, 256, -256, 256, -512, 512);
    m = glm::translate(m, { offset[0], -offset[1], offset[2] });
    m = glm::scale(m, { 1.f, 0.6f, 1.f });
    m = glm::rotate(m, glm::radians(-45.f),  glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::rotate(m, glm::radians(0.0f),   glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, glm::radians(-45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    return Matrix4x4{m};
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
