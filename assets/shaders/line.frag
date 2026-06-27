#version 300 es
// PocketEngine — line/prim fragment shader (GLES 3.00)
precision mediump float;

in vec4 v_color;

out vec4 frag;

void main() {
    frag = v_color;
}
