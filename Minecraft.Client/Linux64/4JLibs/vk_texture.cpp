/*
 * vk_texture.cpp — Vulkan texture creation, upload, descriptor management.
 */

#include "vk_texture.h"
#include "../SessionLog.h"
#include <unordered_map>
#include <cstring>
#include <algorithm>

static std::unordered_map<int, VkTextureInfo> s_textures;
static int s_nextId = 1;

// ─── Helper: one-shot command buffer for image transitions/copies ──────────
static VkCommandBuffer beginOneShot()
{
    VkCommandBufferAllocateInfo ai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    ai.commandPool        = g_vk.commandPool;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(g_vk.device, &ai, &cmd);
    VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    return cmd;
}

static void endOneShot(VkCommandBuffer cmd)
{
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cmd;
    vkQueueSubmit(g_vk.graphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_vk.graphicsQueue);
    vkFreeCommandBuffers(g_vk.device, g_vk.commandPool, 1, &cmd);
}

// ─── Helper: transition image layout ───────────────────────────────────────
static void transitionImage(VkCommandBuffer cmd, VkImage image,
    VkImageLayout oldLayout, VkImageLayout newLayout,
    VkAccessFlags srcAccess, VkAccessFlags dstAccess,
    VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
{
    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout        = oldLayout;
    barrier.newLayout        = newLayout;
    barrier.image            = image;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1 };
    barrier.srcAccessMask    = srcAccess;
    barrier.dstAccessMask    = dstAccess;
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// ─── Helper: update descriptor set with current sampler ────────────────────
static void updateDescriptor(VkTextureInfo& tex)
{
    if (!tex.descSet || !tex.view) return;
    VkDescriptorImageInfo dii = {};
    dii.sampler     = tex.useLinear ? g_pipe.samplerLinear : g_pipe.samplerNearest;
    dii.imageView   = tex.view;
    dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet wds = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    wds.dstSet          = tex.descSet;
    wds.dstBinding      = 0;
    wds.descriptorCount = 1;
    wds.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wds.pImageInfo      = &dii;
    vkUpdateDescriptorSets(g_vk.device, 1, &wds, 0, nullptr);
}

// ─── vkt_create ────────────────────────────────────────────────────────────
int vkt_create()
{
    int id = s_nextId++;
    VkTextureInfo tex = {};

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo dsai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    dsai.descriptorPool     = g_pipe.descPool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts        = &g_pipe.descSetLayout;
    if (vkAllocateDescriptorSets(g_vk.device, &dsai, &tex.descSet) != VK_SUCCESS)
    {
        SessionLog_Printf("[vk] Failed to allocate descriptor set for texture %d\n", id);
        tex.descSet = g_pipe.whiteTexDescSet; // fallback
    }

    s_textures[id] = tex;
    return id;
}

// ─── vkt_free ──────────────────────────────────────────────────────────────
void vkt_free(int id)
{
    auto it = s_textures.find(id);
    if (it == s_textures.end()) return;
    VkTextureInfo& tex = it->second;

    // Wait for GPU to finish using this texture
    vkDeviceWaitIdle(g_vk.device);

    if (tex.descSet && tex.descSet != g_pipe.whiteTexDescSet)
        vkFreeDescriptorSets(g_vk.device, g_pipe.descPool, 1, &tex.descSet);
    if (tex.view)  vkDestroyImageView(g_vk.device, tex.view, nullptr);
    if (tex.image) vmaDestroyImage(g_vk.allocator, tex.image, tex.alloc);

    s_textures.erase(it);
}

// ─── vkt_upload ────────────────────────────────────────────────────────────
void vkt_upload(int id, int width, int height, const void* data, int mipLevel)
{
    auto it = s_textures.find(id);
    if (it == s_textures.end() || !data || width <= 0 || height <= 0) return;
    VkTextureInfo& tex = it->second;

    // If this is mip level 0 (base), (re)create the VkImage
    if (mipLevel == 0)
    {
        // Destroy old image if dimensions changed
        if (tex.image && (tex.width != width || tex.height != height))
        {
            vkDeviceWaitIdle(g_vk.device);
            if (tex.view) vkDestroyImageView(g_vk.device, tex.view, nullptr);
            vmaDestroyImage(g_vk.allocator, tex.image, tex.alloc);
            tex.image = VK_NULL_HANDLE;
            tex.view  = VK_NULL_HANDLE;
        }

        if (!tex.image)
        {
            VkImageCreateInfo ici = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
            ici.imageType     = VK_IMAGE_TYPE_2D;
            ici.format        = VK_FORMAT_B8G8R8A8_UNORM; // matches ARGB int byte order on LE
            ici.extent        = { (uint32_t)width, (uint32_t)height, 1 };
            // Calculate max mip levels from dimensions
            uint32_t maxDim = std::max((uint32_t)width, (uint32_t)height);
            uint32_t mipLevels = 1;
            while (maxDim > 1) { maxDim >>= 1; mipLevels++; }
            ici.mipLevels     = mipLevels;
            ici.arrayLayers   = 1;
            ici.samples       = VK_SAMPLE_COUNT_1_BIT;
            ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
            ici.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            VmaAllocationCreateInfo ai = {};
            ai.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            if (vmaCreateImage(g_vk.allocator, &ici, &ai, &tex.image, &tex.alloc, nullptr) != VK_SUCCESS)
            {
                SessionLog_Printf("[vk] Failed to create image %dx%d for tex %d\n", width, height, id);
                return;
            }

            VkImageViewCreateInfo vci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            vci.image    = tex.image;
            vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vci.format   = VK_FORMAT_B8G8R8A8_UNORM;
            vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1 };
            vkCreateImageView(g_vk.device, &vci, nullptr, &tex.view);

            tex.width  = width;
            tex.height = height;
        }
    }

    if (!tex.image) return;

    // Create staging buffer
    VkDeviceSize imageSize = (VkDeviceSize)width * height * 4;
    VkBuffer staging;
    VmaAllocation stagingAlloc;
    VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size  = imageSize;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo sai = {};
    sai.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    vmaCreateBuffer(g_vk.allocator, &bci, &sai, &staging, &stagingAlloc, nullptr);

    void* mapped;
    vmaMapMemory(g_vk.allocator, stagingAlloc, &mapped);
    memcpy(mapped, data, imageSize);
    vmaUnmapMemory(g_vk.allocator, stagingAlloc);

    // Transfer
    VkCommandBuffer cmd = beginOneShot();

    transitionImage(cmd, tex.image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkBufferImageCopy region = {};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, (uint32_t)mipLevel, 0, 1 };
    region.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };
    vkCmdCopyBufferToImage(cmd, staging, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    transitionImage(cmd, tex.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    endOneShot(cmd);
    vmaDestroyBuffer(g_vk.allocator, staging, stagingAlloc);

    // Update descriptor
    updateDescriptor(tex);
}

// ─── vkt_sub_upload ────────────────────────────────────────────────────────
void vkt_sub_upload(int id, int xoff, int yoff, int width, int height, const void* data, int mipLevel)
{
    auto it = s_textures.find(id);
    if (it == s_textures.end() || !data || !it->second.image) return;
    VkTextureInfo& tex = it->second;

    VkDeviceSize imageSize = (VkDeviceSize)width * height * 4;
    VkBuffer staging;
    VmaAllocation stagingAlloc;
    VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size  = imageSize;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo sai = {};
    sai.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    vmaCreateBuffer(g_vk.allocator, &bci, &sai, &staging, &stagingAlloc, nullptr);

    void* mapped;
    vmaMapMemory(g_vk.allocator, stagingAlloc, &mapped);
    memcpy(mapped, data, imageSize);
    vmaUnmapMemory(g_vk.allocator, stagingAlloc);

    VkCommandBuffer cmd = beginOneShot();

    transitionImage(cmd, tex.image,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkBufferImageCopy region = {};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, (uint32_t)mipLevel, 0, 1 };
    region.imageOffset = { xoff, yoff, 0 };
    region.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };
    vkCmdCopyBufferToImage(cmd, staging, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    transitionImage(cmd, tex.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    endOneShot(cmd);
    vmaDestroyBuffer(g_vk.allocator, staging, stagingAlloc);
}

// ─── vkt_set_filter ────────────────────────────────────────────────────────
void vkt_set_filter(int id, bool linear)
{
    auto it = s_textures.find(id);
    if (it == s_textures.end()) return;
    if (it->second.useLinear != linear)
    {
        it->second.useLinear = linear;
        updateDescriptor(it->second);
    }
}

// ─── vkt_get_descriptor ────────────────────────────────────────────────────
VkDescriptorSet vkt_get_descriptor(int id)
{
    auto it = s_textures.find(id);
    if (it == s_textures.end() || !it->second.view)
        return g_pipe.whiteTexDescSet;
    return it->second.descSet;
}

// ─── vkt_init / vkt_cleanup ────────────────────────────────────────────────
void vkt_init() {}

void vkt_cleanup()
{
    vkDeviceWaitIdle(g_vk.device);
    for (auto& [id, tex] : s_textures)
    {
        if (tex.descSet && tex.descSet != g_pipe.whiteTexDescSet)
            vkFreeDescriptorSets(g_vk.device, g_pipe.descPool, 1, &tex.descSet);
        if (tex.view) vkDestroyImageView(g_vk.device, tex.view, nullptr);
        if (tex.image) vmaDestroyImage(g_vk.allocator, tex.image, tex.alloc);
    }
    s_textures.clear();
}
