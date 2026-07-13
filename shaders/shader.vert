precision highp float;

uniform vec2 scale;
uniform vec2 offset;
uniform vec4 tint;
uniform bool enable_lightmap;
uniform sampler2DArray sampler;
uniform sampler2D lightmap_sampler;

in vec4 position;
in vec3 texcoords;
in vec2 light_coord;
in float depth;

noperspective out vec3 frag_texcoords;
noperspective out vec2 frag_light_coord;

void main() {
    const float factor = 0.5;
    float x = -position.y, y = -position.x, z = position.z;
    gl_Position = vec4((x-y+offset.x)*scale.x, ((x+y+z*2)*factor-offset.y)*scale.y, depth, 1);
    frag_texcoords = texcoords;
    frag_light_coord = light_coord;
}
