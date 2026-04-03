#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

out vec4 finalColor;

#define MAX_LIGHTS 3
#define LIGHT_DIRECTIONAL 1
#define LIGHT_POINT 0

struct Light {
    int enabled;
    int type;
    vec3 position;
    vec3 target;
    vec4 color;
};

uniform Light lights[MAX_LIGHTS];
uniform vec4 ambient;
uniform vec3 viewPos;

const float PI = 3.14159265359;
const float MAX_DIST = 10000.0;

const float R_INNER = 2.0;
const float R = R_INNER + 0.6;

const int NUM_OUT_SCATTER = 8;
const int NUM_IN_SCATTER = 80;

vec2 ray_vs_sphere(vec3 p, vec3 dir, float r)
{
    float b = dot(p, dir);
    float c = dot(p, p) - r * r;

    float d = b * b - c;
    if (d < 0.0) return vec2(MAX_DIST, -MAX_DIST);

    d = sqrt(d);
    return vec2(-b - d, -b + d);
}

float phase_ray(float cc)
{
    return (3.0 / (16.0 * PI)) * (1.0 + cc);
}

float phase_mie(float g, float c, float cc)
{
    float gg = g * g;
    float a = (1.0 - gg) * (1.0 + cc);

    float b = 1.0 + gg - 2.0 * g * c;
    b *= sqrt(b);
    b *= 2.0 + gg;

    return (3.0 / (8.0 * PI)) * a / b;
}

float density(vec3 p, float ph)
{
    return exp(-max(length(p) - R_INNER, 0.0) / ph);
}

float optic(vec3 p, vec3 q, float ph)
{
    vec3 s = (q - p) / float(NUM_OUT_SCATTER);
    vec3 v = p + s * 0.5;

    float sum = 0.0;

    for (int i = 0; i < NUM_OUT_SCATTER; i++)
    {
        sum += density(v, ph);
        v += s;
    }

    return sum * length(s);
}

vec3 in_scatter(vec3 o, vec3 dir, vec2 e, vec3 l)
{
    const float ph_ray = 0.05;
    const float ph_mie = 0.02;

    const vec3 k_ray = vec3(3.8, 13.5, 33.1);
    const vec3 k_mie = vec3(21.0);
    const float k_mie_ex = 1.1;

    vec3 sum_ray = vec3(0.0);
    vec3 sum_mie = vec3(0.0);

    float n_ray0 = 0.0;
    float n_mie0 = 0.0;

    float len = (e.y - e.x) / float(NUM_IN_SCATTER);

    vec3 s = dir * len;
    vec3 v = o + dir * (e.x + len * 0.5);

    for (int i = 0; i < NUM_IN_SCATTER; i++, v += s)
    {
        float d_ray = density(v, ph_ray) * len;
        float d_mie = density(v, ph_mie) * len;

        n_ray0 += d_ray;
        n_mie0 += d_mie;

        vec2 f = ray_vs_sphere(v, l, R);
        vec3 u = v + l * f.y;

        float n_ray1 = optic(v, u, ph_ray);
        float n_mie1 = optic(v, u, ph_mie);

        vec3 att = exp(-(n_ray0 + n_ray1) * k_ray - (n_mie0 + n_mie1) * k_mie * k_mie_ex);

        sum_ray += d_ray * att;
        sum_mie += d_mie * att;
    }

    float c = dot(dir, -l);
    float cc = c * c;

    vec3 scatter =
        sum_ray * k_ray * phase_ray(cc) +
            sum_mie * k_mie * phase_mie(-0.78, c, cc);

    return 10.0 * scatter;
}

void main()
{
    vec3 rayOrigin = viewPos;
    vec3 rayDir = normalize(fragPosition - viewPos);

    // Use first directional light as the sun
    vec3 sunDir = normalize(lights[0].position - lights[0].target);

    vec2 e = ray_vs_sphere(rayOrigin, rayDir, R);

    if (e.x > e.y)
    {
        discard;
    }

    vec2 f = ray_vs_sphere(rayOrigin, rayDir, R_INNER);
    e.y = min(e.y, f.x);

    vec3 scatter = in_scatter(rayOrigin, rayDir, e, sunDir);

    vec4 texelColor = texture(texture0, fragTexCoord);
    vec4 tint = colDiffuse * fragColor;

    vec3 color = texelColor.rgb * scatter;

    finalColor = vec4(color, texelColor.a * 0.3) * tint;

    finalColor = pow(finalColor, vec4(1.0 / 2.2));
}
