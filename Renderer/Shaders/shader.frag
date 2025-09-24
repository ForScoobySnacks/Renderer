#version 450

layout(location = 1) in vec2 fragTex;
layout(location = 3) in vec3 Normal;

layout(set = 1, binding = 0) uniform sampler2D textureSampler;

layout(std140, set = 0, binding = 1) uniform Light {
    vec4 lightDir;   
    vec4 lightCol;   
} light;

layout(location = 0) out vec4 outColour;

void main() {
    vec3 N = normalize(Normal);              
    vec3 L = normalize(-light.lightDir.xyz);      
    float NdotL = max(dot(N, L), 0.0);

    vec4 tex = texture(textureSampler, fragTex);
    vec3 diffuse = light.lightCol.rgb * tex.rgb * NdotL;

    vec3 ambient = 0.1 * tex.rgb;
    vec3 color = ambient + diffuse;

    outColour = vec4(color, tex.a);
}
