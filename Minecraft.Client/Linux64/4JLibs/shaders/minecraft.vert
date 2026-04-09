#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in uint inColorPacked; // 0xRRGGBBAA

layout(push_constant) uniform PushConstants {
    mat4 mvp;            // 64 bytes
    vec4 globalColor;    // 16 bytes — current glColor state for non-vertex-color draws
    vec4 fogColor;       // 16 bytes
    vec4 fogParams;      // x=near, y=far, z=density, w=mode (0=linear, 1=exp)
    float alphaRef;      // 4 bytes
    float fogEnable;     // 4 bytes
    float alphaTestEnable; // 4 bytes
    float textureEnable;   // 4 bytes
} pc;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;
layout(location = 2) out float v_fogFactor;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    v_uv = inTexCoord;

    // Unpack 0xRRGGBBAA from uint32
    v_color = vec4(
        float((inColorPacked >> 24u) & 0xFFu) / 255.0,
        float((inColorPacked >> 16u) & 0xFFu) / 255.0,
        float((inColorPacked >> 8u)  & 0xFFu) / 255.0,
        float( inColorPacked         & 0xFFu) / 255.0
    );

    // Fog factor (use eye-space distance: gl_Position.w = -z_eye for RH projection)
    if (pc.fogEnable > 0.5) {
        float dist = gl_Position.w;
        if (pc.fogParams.w < 0.5) {
            // Linear fog
            v_fogFactor = clamp((pc.fogParams.y - dist) / (pc.fogParams.y - pc.fogParams.x + 0.001), 0.0, 1.0);
        } else {
            // Exponential fog
            v_fogFactor = clamp(exp(-pc.fogParams.z * dist), 0.0, 1.0);
        }
    } else {
        v_fogFactor = 1.0;
    }
}
