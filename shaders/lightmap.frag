precision mediump float;

struct light_u
{
    vec4 color_and_intensity;
    vec2 center;
    uint mode;
};

#define TILE_MAX_DIM 16
#define TILE_SIZE_X 64
#define TILE_SIZE_Y 64

#define CHUNK_SIZE_X (TILE_SIZE_X * TILE_MAX_DIM)
#define CHUNK_SIZE_Y (TILE_SIZE_Y * TILE_MAX_DIM)

layout (location = 0) uniform vec4 color_intensity;
layout (location = 1) uniform vec2 px;
