#version 300 es
// PocketEngine — sprite fragment shader (GLES 3.00)
precision mediump float;

uniform sampler2D u_tex;

in vec2 v_uv;
in vec4 v_color;

out vec4 frag;

void main() {
    frag = texture(u_tex, v_uv) * v_color;
}
