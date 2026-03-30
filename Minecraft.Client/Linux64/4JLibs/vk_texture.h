#pragma once
/*
 * vk_texture.h — Vulkan texture management.
 * Creates VkImage + VkImageView + VkDescriptorSet per game texture.
 * Handles ARGB int pixel data (BGRA byte order on LE).
 */

#include "vk_backend.h"
#include "vk_pipeline.h"

struct VkTextureInfo
{
    VkImage         image    = VK_NULL_HANDLE;
    VkImageView     view     = VK_NULL_HANDLE;
    VmaAllocation   alloc    = VK_NULL_HANDLE;
    VkDescriptorSet descSet  = VK_NULL_HANDLE;
    int             width    = 0;
    int             height   = 0;
    bool            useLinear = false; // false=nearest, true=linear
};

// Create a new texture entry, returns internal ID. Does NOT upload data yet.
int  vkt_create();

// Free a texture and its Vulkan resources.
void vkt_free(int id);

// Upload full image data (ARGB ints, BGRA byte order).
void vkt_upload(int id, int width, int height, const void* data, int mipLevel);

// Upload sub-region update.
void vkt_sub_upload(int id, int xoff, int yoff, int width, int height, const void* data, int mipLevel);

// Set filter mode (nearest or linear).
void vkt_set_filter(int id, bool linear);

// Get the VkDescriptorSet for a texture (for binding before draw).
VkDescriptorSet vkt_get_descriptor(int id);

// Lifecycle
void vkt_init();
void vkt_cleanup();
