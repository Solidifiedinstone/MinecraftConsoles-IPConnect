#pragma once
/*
 * vk_pipeline.h — Vulkan graphics pipeline management.
 * Creates shader modules, pipeline layout, descriptor set layout, and
 * pipeline cache with variants for different blend/depth/cull states.
 */

#include "vk_backend.h"
#include <unordered_map>
#include <cstdint>

// Push constant layout — must match shaders exactly (128 bytes).
struct VkPushConstants
{
    float mvp[16];        // 64 bytes — mat4
    float globalColor[4]; // 16 bytes — vec4
    float fogColor[4];    // 16 bytes — vec4
    float fogParams[4];   // 16 bytes — x=near, y=far, z=density, w=mode
    float alphaRef;       //  4 bytes
    float fogEnable;      //  4 bytes
    float alphaTestEnable; // 4 bytes
    float textureEnable;   // 4 bytes
};                         // = 128 bytes total
static_assert(sizeof(VkPushConstants) == 128, "Push constants must be exactly 128 bytes");

// Pipeline key — packed static state that requires different pipeline objects.
struct VkPipelineKey
{
    uint8_t blendEnable    : 1;
    uint8_t srcBlend       : 4; // VkBlendFactor (first 16 values cover all used)
    uint8_t dstBlend       : 4; // VkBlendFactor
    uint8_t topology       : 3; // 0=tri list, 1=line list, 2=line strip, 3=tri strip, 4=tri fan
    uint8_t colorWriteMask : 4; // RGBA bits
    uint8_t depthTestEnable  : 1;
    uint8_t depthWriteEnable : 1;

    uint32_t pack() const {
        return (uint32_t)blendEnable | ((uint32_t)srcBlend << 1) | ((uint32_t)dstBlend << 5) |
               ((uint32_t)topology << 9) | ((uint32_t)colorWriteMask << 12) |
               ((uint32_t)depthTestEnable << 16) | ((uint32_t)depthWriteEnable << 17);
    }
    bool operator==(const VkPipelineKey& o) const { return pack() == o.pack(); }
};
struct VkPipelineKeyHash {
    size_t operator()(const VkPipelineKey& k) const { return k.pack(); }
};

struct VkPipelineState
{
    // Shader modules
    VkShaderModule          vertModule = VK_NULL_HANDLE;
    VkShaderModule          fragModule = VK_NULL_HANDLE;

    // Layout
    VkDescriptorSetLayout   descSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout        pipelineLayout = VK_NULL_HANDLE;

    // Descriptor pool & white fallback texture
    VkDescriptorPool        descPool = VK_NULL_HANDLE;
    VkDescriptorSet         whiteTexDescSet = VK_NULL_HANDLE;
    VkImage                 whiteTexImage = VK_NULL_HANDLE;
    VkImageView             whiteTexView = VK_NULL_HANDLE;
    VmaAllocation           whiteTexAlloc = VK_NULL_HANDLE;
    VkSampler               samplerNearest = VK_NULL_HANDLE;
    VkSampler               samplerLinear = VK_NULL_HANDLE;

    // Pipeline cache
    std::unordered_map<VkPipelineKey, VkPipeline, VkPipelineKeyHash> cache;
};

extern VkPipelineState g_pipe;

// Lifecycle
bool vkp_init();
void vkp_cleanup();

// Get or create a pipeline for the given state key
VkPipeline vkp_get_pipeline(const VkPipelineKey& key);

// Get the default pipeline key (opaque, triangle list, all color channels)
VkPipelineKey vkp_default_key();
