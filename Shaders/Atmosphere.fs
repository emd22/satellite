#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

// NOTE: Add your custom variables here

#define     MAX_LIGHTS              3
#define     LIGHT_DIRECTIONAL       1
#define     LIGHT_POINT             0

struct Light {
    int enabled;
    int type;
    vec3 position;
    vec3 target;
    vec4 color;
};

// Input lighting values
uniform Light lights[MAX_LIGHTS];
uniform vec4 ambient;
uniform vec3 viewPos;

void main()
{
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 lightDot = vec3(0.0);
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(viewPos - fragPosition);
    vec3 specular = vec3(0.0);

    vec4 tint = colDiffuse * fragColor;

    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (lights[i].enabled == 1)
        {
            vec3 light = vec3(0.0);

            if (lights[i].type == LIGHT_DIRECTIONAL)
            {
                light = -normalize(lights[i].target - lights[i].position);
            }

            if (lights[i].type == LIGHT_POINT)
            {
                light = normalize(lights[i].position - fragPosition);
            }

            float NdotL = max(dot(N, light), 0.0);
            lightDot += lights[i].color.rgb * NdotL;

            float specCo = 0.0;
            if (NdotL > 0.0) specCo = pow(max(0.0, dot(V, reflect(-(light), N))), 50.0); // 16 refers to shine
            specular += specCo;
        }
    }

    float rimFactor = 1.0 - max(dot(N, V), 0.0);

    float rimPower = 5.0;

    float rimLighting = pow(rimFactor, rimPower);

    vec3 rimColor = vec3(0.4, 0.7, 0.9);
    vec3 rimFinal = (rimLighting * rimColor);

    finalColor = vec4(texelColor);

    vec4 lightFinal = (((tint + vec4(specular, 1.0)) * vec4(lightDot, 1.0)));
    lightFinal += (vec4(0.5) / 10.0) * tint;

    finalColor = vec4(rimFinal, length(lightFinal.rgb));

    // finalColor = pow(finalColor, vec4(1.0 / 2.2));
}
