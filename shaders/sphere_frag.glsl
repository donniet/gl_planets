precision mediump float;

uniform vec3 camera;
uniform sampler2D texture;
uniform sampler2D starfield;
uniform vec3 sun;

varying vec3 direction;

vec4 textureSphere(sampler2D texture, vec3 p) {
  vec2 s = vec2(atan(p.x, p.z), -asin(p.y));
  /* [ \frac{1}{2\pi}, \frac{1}{\pi} ] */
  s *= vec2(0.1591549430919, 0.31830988618379);

  return texture2D(texture, vec2(s.s + 0.5, s.t + 0.5));
}

void swap(inout float a, inout float b) {
  float c = a;
  a = b;
  b = c;
}

bool solveQuadratic(float a, float b, float c, out float x0, out float x1, out float discr) {
  discr = b * b - 4. * a * c;
  bool ret = discr > 0.;

  if (discr < 0.) discr = -discr;
  
  if (discr == 0.) {
    x0 = x1 = -0.5 * b / a;
  } else {
    float q;
    if (b > 0.) {
      q = -0.5 * (b + sqrt(discr));
    } else {
      q = -0.5 * (b - sqrt(discr));
    }
    x0 = q / a;
    x1 = c / q;

    if (x0 > x1) swap(x0, x1);
  }

  if(!ret) discr = -discr;  

  return ret;
}

// https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
bool rayIntersectsSphere(vec3 orig, vec3 dir, vec3 center, float radius, out vec3 inter, out vec3 n) {
  float r2 = radius * radius;

  vec3 L = orig - center;
  float a = dot(dir, dir);
  float b = 2. * dot(dir, L);
  float c = dot(L, L) - r2;

  float t0, t1, discr;
  solveQuadratic(a, b, c, t0, t1, discr);

  if(discr < 0.) {
    return false;
  }

  if (t0 < 0.) {
    t0 = t1;
    if (t0 < 0.) return false;
  }

  inter = orig + t0 * dir;
  n = normalize(inter - center);

  return true;
}

void main() {
  vec3 inter = vec3(0.,0.,0.);
  vec3 n = vec3(0., 0., 0.);
  vec3 center = vec3(0., 0., 0.);
  if (rayIntersectsSphere(camera, direction, center, 1., inter, n)) {
    float shade = max(dot(sun, n), 0.) + 0.05;
    vec4 col = textureSphere(texture, n);
    gl_FragColor = vec4(shade * col.xyz, col.w);
  } else {
    gl_FragColor = textureSphere(starfield, direction);
    // gl_FragColor = vec4(0.,0.,0.,1.);
  }

}