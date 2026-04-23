#pragma once
#include <mg/Mesh.h>
#include <mg/TextureArray.h>

namespace floormat { struct tile_shader; }

namespace floormat::wireframe {

GL::Texture2DArray make_constant_texture();

struct mesh_base
{
    static void set_line_width(float width);

protected:
    GL::Buffer _vertex_buffer{{}, GL::BufferUsage::DynamicDraw}, _constant_buffer, _index_buffer;
    GL::Texture2DArray* _texture;
    GL::Mesh _mesh;

    mesh_base(GL::MeshPrimitive primitive, ArrayView<const void> index_data,
              size_t num_vertices, size_t num_indexes, GL::Texture2DArray* texture);
    void draw(tile_shader& shader);
    void set_subdata(ArrayView<const void> array);
};

template<typename T>
struct wireframe_mesh final : private wireframe::mesh_base
{
    wireframe_mesh(GL::Texture2DArray& constant_texture);
    void draw(tile_shader& shader, T traits);
};

template<typename T>
wireframe_mesh<T>::wireframe_mesh(GL::Texture2DArray& constant_texture) :
    wireframe::mesh_base{T::primitive, T::make_index_array(), T::num_vertices, T::num_indexes, &constant_texture}
{
}

template <typename T> void wireframe_mesh<T>::draw(tile_shader& shader, T x)
{
    //_texcoord_buffer.setData({nullptr, sizeof(Vector3) * T::num_vertices}, GL::BufferUsage::DynamicDraw); // orphan the buffer
    set_subdata(x.make_vertex_array());
    x.on_draw();
    mesh_base::draw(shader);
}

} // namespace floormat::wireframe
