#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 2) in float v_fogFactor;

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 globalColor;
    vec4 fogColor;
    vec4 fogParams;
    float alphaRef;
    float fogEnable;
    float alphaTestEnable;
    float textureEnable;
} pc;

layout(location = 0) out vec4 fragColor;

void main() {
    vec4 texColor = (pc.textureEnable > 0.5) ? texture(texSampler, v_uv) : vec4(1.0);
    vec4 color = texColor * v_color * pc.globalColor;

    // Alpha test
    if (pc.alphaTestEnable > 0.5 && color.a <= pc.alphaRef)
        discard;

    // Fog
    if (pc.fogEnable > 0.5)
        color.rgb = mix(pc.fogColor.rgb, color.rgb, v_fogFactor);

    fragColor = color;
}
