#include "chunk.hpp"

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/Utility/Resource.h>
#include <Magnum/ImageView.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "tile-shader.hpp"

namespace Magnum::Examples {

struct application : Platform::Application {
    explicit application(const Arguments& arguments);
    void drawEvent() override;

    const Utility::Resource rs{"texturedquad-data"};
    PluginManager::Manager<Trade::AbstractImporter> plugins;
    Containers::Pointer<Trade::AbstractImporter> tga_importer =
        plugins.loadAndInstantiate("TgaImporter");

    GL::Mesh _mesh;
    tile_shader _shader;
    atlas_texture atlas = make_atlas("images/tiles.tga", {8, 4});

    atlas_texture make_atlas(const std::string& file, Vector2i dims)
    {
        if(!tga_importer || !tga_importer->openFile(file))
            std::exit(1);

        Containers::Optional<Trade::ImageData2D> image = tga_importer->image2D(0);
        CORRADE_INTERNAL_ASSERT(image);

        return atlas_texture{*image, dims};
    }
};

application::application(const Arguments& arguments):
    Platform::Application{arguments, Configuration{}
        .setTitle("Magnum Textured Quad Example")
        .setSize({512, 512})}
{
    struct QuadVertex {
        Vector3 position;
        Vector2 textureCoordinates;
    };
    QuadVertex vertices[4];
    auto positions = atlas.floor_quad({}, {2, 2});
    auto texcoords = atlas.texcoords_for_id(2);
    auto indices   = atlas.indices(0);

    for (unsigned i = 0; i < std::size(vertices); i++)
        vertices[i] = { positions[i], texcoords[i] };

    _mesh.setCount((int)std::size(indices))
        .addVertexBuffer(GL::Buffer{vertices}, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{})
        .setIndexBuffer(GL::Buffer{indices}, 0,
            GL::MeshIndexType::UnsignedShort);
}

void application::drawEvent() {
    GL::defaultFramebuffer.clear(GL::FramebufferClear::Color);

    using namespace Math::Literals;

    _shader
        .setColor(0xffffff_rgbf)
        .bindTexture(atlas.texture())
        .draw(_mesh);

    swapBuffers();
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
