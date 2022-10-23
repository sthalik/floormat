#pragma once
#include "draw/wireframe-mesh.hpp"
#include "draw/wireframe-quad.hpp"
#include "draw/wireframe-box.hpp"
#include "main/floormat-app.hpp"

namespace floormat {

struct app final : floormat_app
{
    app();
    ~app() override;

private:
    wireframe_mesh<wireframe::quad> _wireframe_quad;
    wireframe_mesh<wireframe::box> _wireframe_box;
};

} // namespace floormat
