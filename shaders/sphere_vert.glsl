precision mediump float;

attribute vec2 corner;
uniform mat4 inv;

varying vec3 direction;

void main() {
  gl_Position = vec4(corner, 1.0, 1.0);
  direction = (inv * gl_Position).xyz;
}