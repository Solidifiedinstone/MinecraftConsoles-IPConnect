/*
 * vk_pipeline.cpp — Vulkan pipeline creation and caching.
 */

#include "vk_pipeline.h"
#include "../SessionLog.h"
#include <cstring>

// Embedded SPIR-V bytecode
#include "shaders/shader_vert.h"
#include "shaders/shader_frag.h"

VkPipelineState g_pipe;

// ─── Create 1x1 white texture as fallback ──────────────────────────────────
static bool createWhiteTexture()
{
    // Create image
    VkImageCreateInfo ici = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent        = { 1, 1, 1 };
    ici.mipLevels     = 1;
    ici.arrayLayers   = 1;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VmaAllocationCreateInfo ai = {};
    ai.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaCreateImage(g_vk.allocator, &ici, &ai, &g_pipe.whiteTexImage, &g_pipe.whiteTexAlloc, nullptr);

    // Image view
    VkImageViewCreateInfo vci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    vci.image    = g_pipe.whiteTexImage;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format   = VK_FORMAT_R8G8B8A8_UNORM;
    vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCreateImageView(g_vk.device, &vci, nullptr, &g_pipe.whiteTexView);

    // Upload white pixel via staging
    uint32_t white = 0xFFFFFFFF;
    VkBuffer staging;
    VmaAllocation stagingAlloc;
    VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size  = 4;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo sai = {};
    sai.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    vmaCreateBuffer(g_vk.allocator, &bci, &sai, &staging, &stagingAlloc, nullptr);
    void* mapped;
    vmaMapMemory(g_vk.allocator, stagingAlloc, &mapped);
    memcpy(mapped, &white, 4);
    vmaUnmapMemory(g_vk.allocator, stagingAlloc);

    // One-shot command buffer for upload
    VkCommandBufferAllocateInfo cba = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cba.commandPool = g_vk.commandPool;
    cba.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cba.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(g_vk.device, &cba, &cmd);
    VkCommandBufferBeginInfo cbi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &cbi);

    // Transition to transfer dst
    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image     = g_pipe.whiteTexImage;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region = {};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageExtent = { 1, 1, 1 };
    vkCmdCopyBufferToImage(cmd, staging, g_pipe.whiteTexImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition to shader read
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);
    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(g_vk.graphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_vk.graphicsQueue);
    vkFreeCommandBuffers(g_vk.device, g_vk.commandPool, 1, &cmd);
    vmaDestroyBuffer(g_vk.allocator, staging, stagingAlloc);

    // Allocate descriptor set for white texture
    VkDescriptorSetAllocateInfo dsai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    dsai.descriptorPool     = g_pipe.descPool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts        = &g_pipe.descSetLayout;
    vkAllocateDescriptorSets(g_vk.device, &dsai, &g_pipe.whiteTexDescSet);

    VkDescriptorImageInfo dii = {};
    dii.sampler     = g_pipe.samplerNearest;
    dii.imageView   = g_pipe.whiteTexView;
    dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet wds = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    wds.dstSet          = g_pipe.whiteTexDescSet;
    wds.dstBinding      = 0;
    wds.descriptorCount = 1;
    wds.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wds.pImageInfo      = &dii;
    vkUpdateDescriptorSets(g_vk.device, 1, &wds, 0, nullptr);

    return true;
}

// ─── vkp_init ──────────────────────────────────────────────────────────────
bool vkp_init()
{
    // Shader modules
    g_pipe.vertModule = vkb_create_shader_module(
        (const uint32_t*)minecraft_vert_spv, minecraft_vert_spv_len);
    g_pipe.fragModule = vkb_create_shader_module(
        (const uint32_t*)minecraft_frag_spv, minecraft_frag_spv_len);
    if (!g_pipe.vertModule || !g_pipe.fragModule)
    {
        SessionLog_Printf("[vk] Failed to create shader modules\n");
        return false;
    }

    // Samplers
    VkSamplerCreateInfo sci = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sci.magFilter    = VK_FILTER_NEAREST;
    sci.minFilter    = VK_FILTER_NEAREST;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkCreateSampler(g_vk.device, &sci, nullptr, &g_pipe.samplerNearest);
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(g_vk.device, &sci, nullptr, &g_pipe.samplerLinear);

    // Descriptor set layout: binding 0 = combined image sampler
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding         = 0;
    binding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dslci = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    dslci.bindingCount = 1;
    dslci.pBindings    = &binding;
    vkCreateDescriptorSetLayout(g_vk.device, &dslci, nullptr, &g_pipe.descSetLayout);

    // Pipeline layout: push constants (128 bytes) + 1 descriptor set
    VkPushConstantRange pcRange = {};
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcRange.offset     = 0;
    pcRange.size       = sizeof(VkPushConstants);

    VkPipelineLayoutCreateInfo plci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    plci.setLayoutCount         = 1;
    plci.pSetLayouts            = &g_pipe.descSetLayout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges    = &pcRange;
    vkCreatePipelineLayout(g_vk.device, &plci, nullptr, &g_pipe.pipelineLayout);

    // Descriptor pool (512 textures)
    VkDescriptorPoolSize poolSize = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512 };
    VkDescriptorPoolCreateInfo dpci = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    dpci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dpci.maxSets       = 512;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes    = &poolSize;
    vkCreateDescriptorPool(g_vk.device, &dpci, nullptr, &g_pipe.descPool);

    // White fallback texture
    createWhiteTexture();

    SessionLog_Printf("[vk] Pipeline initialized (shaders + layout + descriptors)\n");
    return true;
}

// ─── vkp_get_pipeline ──────────────────────────────────────────────────────
VkPipeline vkp_get_pipeline(const VkPipelineKey& key)
{
    auto it = g_pipe.cache.find(key);
    if (it != g_pipe.cache.end())
        return it->second;

    // Create new pipeline variant
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = g_pipe.vertModule;
    stages[0].pName  = "main";
    stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = g_pipe.fragModule;
    stages[1].pName  = "main";

    // Vertex input: pos(float3), uv(float2), color(uint32)
    VkVertexInputBindingDescription vertBinding = {};
    vertBinding.binding   = 0;
    vertBinding.stride    = 32; // 8 x uint32
    vertBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[3] = {};
    attrs[0].location = 0; attrs[0].binding = 0; attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT; attrs[0].offset = 0;   // pos
    attrs[1].location = 1; attrs[1].binding = 0; attrs[1].format = VK_FORMAT_R32G32_SFLOAT;    attrs[1].offset = 12;  // uv
    attrs[2].location = 2; attrs[2].binding = 0; attrs[2].format = VK_FORMAT_R32_UINT;         attrs[2].offset = 20;  // color packed

    VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertexInput.vertexBindingDescriptionCount   = 1;
    vertexInput.pVertexBindingDescriptions      = &vertBinding;
    vertexInput.vertexAttributeDescriptionCount = 3;
    vertexInput.pVertexAttributeDescriptions    = attrs;

    // Input assembly
    VkPrimitiveTopology topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    if (key.topology == 1) topo = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    else if (key.topology == 2) topo = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    else if (key.topology == 3) topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    else if (key.topology == 4) topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    inputAssembly.topology = topo;

    // Viewport/scissor (dynamic)
    VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    // Rasterization
    VkPipelineRasterizationStateCreateInfo raster = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode    = VK_CULL_MODE_NONE; // dynamic
    raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth   = 1.0f;

    // Multisample
    VkPipelineMultisampleStateCreateInfo multisample = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth/stencil (dynamic test/write/compare)
    VkPipelineDepthStencilStateCreateInfo depthStencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depthStencil.depthTestEnable  = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;

    // Color blend
    VkPipelineColorBlendAttachmentState blendAttach = {};
    blendAttach.colorWriteMask = key.colorWriteMask & 0x0F
        ? (VkColorComponentFlags)key.colorWriteMask
        : (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
    blendAttach.blendEnable = key.blendEnable ? VK_TRUE : VK_FALSE;
    if (key.blendEnable)
    {
        blendAttach.srcColorBlendFactor = (VkBlendFactor)key.srcBlend;
        blendAttach.dstColorBlendFactor = (VkBlendFactor)key.dstBlend;
        blendAttach.colorBlendOp        = VK_BLEND_OP_ADD;
        blendAttach.srcAlphaBlendFactor = (VkBlendFactor)key.srcBlend;
        blendAttach.dstAlphaBlendFactor = (VkBlendFactor)key.dstBlend;
        blendAttach.alphaBlendOp        = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo colorBlend = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments    = &blendAttach;

    // Dynamic state
    VkDynamicState dynStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH,
        VK_DYNAMIC_STATE_DEPTH_BIAS,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS,
    };
    VkPipelineDynamicStateCreateInfo dynState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynState.dynamicStateCount = 5;
    dynState.pDynamicStates    = dynStates;

    // Create pipeline
    VkGraphicsPipelineCreateInfo pci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pci.stageCount          = 2;
    pci.pStages             = stages;
    pci.pVertexInputState   = &vertexInput;
    pci.pInputAssemblyState = &inputAssembly;
    pci.pViewportState      = &viewportState;
    pci.pRasterizationState = &raster;
    pci.pMultisampleState   = &multisample;
    pci.pDepthStencilState  = &depthStencil;
    pci.pColorBlendState    = &colorBlend;
    pci.pDynamicState       = &dynState;
    pci.layout              = g_pipe.pipelineLayout;
    pci.renderPass          = g_vk.renderPass;
    pci.subpass             = 0;

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(g_vk.device, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline) != VK_SUCCESS)
    {
        SessionLog_Printf("[vk] Failed to create pipeline variant\n");
        return VK_NULL_HANDLE;
    }

    g_pipe.cache[key] = pipeline;
    return pipeline;
}

// ─── vkp_default_key ───────────────────────────────────────────────────────
VkPipelineKey vkp_default_key()
{
    VkPipelineKey key = {};
    key.blendEnable    = 0;
    key.srcBlend       = VK_BLEND_FACTOR_SRC_ALPHA;
    key.dstBlend       = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    key.topology       = 0; // triangle list
    key.colorWriteMask = 0xF; // RGBA
    return key;
}

// ─── vkp_cleanup ───────────────────────────────────────────────────────────
void vkp_cleanup()
{
    for (auto& [k, p] : g_pipe.cache)
        vkDestroyPipeline(g_vk.device, p, nullptr);
    g_pipe.cache.clear();

    if (g_pipe.whiteTexView)  vkDestroyImageView(g_vk.device, g_pipe.whiteTexView, nullptr);
    if (g_pipe.whiteTexImage) vmaDestroyImage(g_vk.allocator, g_pipe.whiteTexImage, g_pipe.whiteTexAlloc);
    if (g_pipe.descPool)      vkDestroyDescriptorPool(g_vk.device, g_pipe.descPool, nullptr);
    if (g_pipe.pipelineLayout) vkDestroyPipelineLayout(g_vk.device, g_pipe.pipelineLayout, nullptr);
    if (g_pipe.descSetLayout)  vkDestroyDescriptorSetLayout(g_vk.device, g_pipe.descSetLayout, nullptr);
    if (g_pipe.samplerNearest) vkDestroySampler(g_vk.device, g_pipe.samplerNearest, nullptr);
    if (g_pipe.samplerLinear)  vkDestroySampler(g_vk.device, g_pipe.samplerLinear, nullptr);
    if (g_pipe.vertModule) vkDestroyShaderModule(g_vk.device, g_pipe.vertModule, nullptr);
    if (g_pipe.fragModule) vkDestroyShaderModule(g_vk.device, g_pipe.fragModule, nullptr);
    g_pipe = {};
}
