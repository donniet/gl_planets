precision mediump float;

uniform vec3 camera;
uniform sampler2D texture;
uniform sampler2D starfield;
uniform sampler2D dem;
uniform sampler2D norm;
uniform vec3 sun;

varying vec3 direction;

#define saturate(x) clamp(x, 0.0, 1.0)
#define PI 3.14159265359

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

    if (discr < 0.) return false;

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

    return true;
}

// https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
bool rayIntersectsSphere(vec3 orig, vec3 dir, vec3 center, float radius, out vec3 inter, out vec3 n, out vec3 t, out vec3 b) {
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

    // texture coords are x == lon, y == lat

    float lon = atan(n.x, n.z) + 0.1;
    float r = sqrt(n.x * n.x + n.z * n.z);
    vec3 n1 = vec3(r * sin(lon), n.y, r * cos(lon));
    t = normalize(n1 - n);
    b = cross(n, t);

    return true;
}

// from https://www.shadertoy.com/view/XlKSDR

float pow5(float x) {
    float x2 = x * x;
    return x2 * x2 * x;
}

float D_GGX(float linearRoughness, float NoH, const vec3 h) {
    // Walter et al. 2007, "Microfacet Models for Refraction through Rough Surfaces"
    float oneMinusNoHSquared = 1.0 - NoH * NoH;
    float a = NoH * linearRoughness;
    float k = linearRoughness / (oneMinusNoHSquared + a * a);
    float d = k * k * (1.0 / PI);
    return d;
}

float V_SmithGGXCorrelated(float linearRoughness, float NoV, float NoL) {
    // Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
    float a2 = linearRoughness * linearRoughness;
    float GGXV = NoL * sqrt((NoV - a2 * NoV) * NoV + a2);
    float GGXL = NoV * sqrt((NoL - a2 * NoL) * NoL + a2);
    return 0.5 / (GGXV + GGXL);
}

vec3 F_Schlick(const vec3 f0, float VoH) {
    // Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"
    return f0 + (vec3(1.0) - f0) * pow5(1.0 - VoH);
}

float F_Schlick(float f0, float f90, float VoH) {
    return f0 + (f90 - f0) * pow5(1.0 - VoH);
}

float Fd_Burley(float linearRoughness, float NoV, float NoL, float LoH) {
    // Burley 2012, "Physically-Based Shading at Disney"
    float f90 = 0.5 + 2.0 * linearRoughness * LoH * LoH;
    float lightScatter = F_Schlick(1.0, f90, NoL);
    float viewScatter  = F_Schlick(1.0, f90, NoV);
    return lightScatter * viewScatter * (1.0 / PI);
}


float Fd_Lambert() {
    return 1.0 / PI;
}

void main() {
    vec3 inter = vec3(0.,0.,0.);
    vec3 n = vec3(0., 0., 0.);
    vec3 b, t;
    vec3 center = vec3(0., 0., 0.);
    vec3 d = normalize(direction);

    vec3 v = -d;
    vec3 l = normalize(sun);
    vec3 h = normalize(v + l);
    vec3 r = normalize(reflect(direction, n));

    float metallic = 0.2;
    float roughness = 0.8;
    float intensity = 2.0;

    float linearRoughness = roughness * roughness;


    if (rayIntersectsSphere(camera, d, center, 1., inter, n, t, b)) {
        vec3 nm = normalize(textureSphere(norm, n).xyz * 0.5 - 0.5);

        // mat3 tbn = mat3(t.x, b.x, n.x, t.y, b.y, n.y, t.z, b.z, n.z);
        mat3 tbn = mat3(t.x, t.y, t.z, b.x, b.y, b.z, n.x, n.y, n.z);
        // tbn = transpose(tbn);

        vec3 light_n = normalize(n + 0.5 * tbn * nm);

        float NoV = abs(dot(light_n, v)) + 1e-5;
        float NoL = saturate(dot(light_n, l));
        float NoH = saturate(dot(light_n, h));
        float LoH = saturate(dot(l, h));

        vec3 baseColor = textureSphere(texture, n).rgb;
        vec3 diffuseColor = (1.0 - metallic) * baseColor.rgb;
        vec3 f0 = 0.04 * (1.0 - metallic) + baseColor.rgb * metallic;

        //TODO: add shadows
        // float attenuation = shadow(vec3(0,0,0), l);
        float attenuation = 1.0;

        // specular BRDF
        float D = D_GGX(linearRoughness, NoH, h);
        float V = V_SmithGGXCorrelated(linearRoughness, NoV, NoL);
        vec3  F = F_Schlick(f0, LoH);
        vec3 Fr = (D * V) * F;
        // diffuse BRDF
        vec3 Fd = diffuseColor * Fd_Burley(linearRoughness, NoV, NoL, LoH);

        vec3 color = Fd + Fr;
        // color *= intensity;
        color *= (intensity * attenuation * NoL) * vec3(0.98, 0.92, 0.89);

        // float shade = max(dot(sun, n), 0.) + 0.05;
        vec4 col = textureSphere(texture, n);
        gl_FragColor = vec4(color.rgb, 1.0);
       return;
    } else {
        gl_FragColor = textureSphere(starfield, d); 
    }

}