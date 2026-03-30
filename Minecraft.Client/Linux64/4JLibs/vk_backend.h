#pragma once
/*
 * vk_backend.h — Vulkan rendering backend for Minecraft Console Edition Linux port.
 *
 * Manages: VkInstance, VkDevice, VkSwapchain, VkRenderPass, depth buffer,
 *          command buffers, frame synchronization, VMA allocator.
 *
 * Called from 4J_Stubs.cpp in place of OpenGL calls.
 */

#include <vulkan/vulkan.h>
#include "vma/vk_mem_alloc.h"
#include <vector>
#include <GLFW/glfw3.h>

// Maximum frames that can be in-flight simultaneously.
static constexpr int VKB_MAX_FRAMES_IN_FLIGHT = 2;

// Per-frame staging buffer for vertex data (16 MB).
static constexpr VkDeviceSize VKB_STAGING_BUFFER_SIZE = 16 * 1024 * 1024;

struct VkBackend
{
    // Core
    VkInstance               instance         = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger   = VK_NULL_HANDLE;
    VkPhysicalDevice         physicalDevice   = VK_NULL_HANDLE;
    VkDevice                 device           = VK_NULL_HANDLE;
    VkQueue                  graphicsQueue    = VK_NULL_HANDLE;
    uint32_t                 graphicsFamily   = 0;
    VkSurfaceKHR             surface          = VK_NULL_HANDLE;
    VmaAllocator             allocator        = VK_NULL_HANDLE;

    // Swapchain
    VkSwapchainKHR           swapchain        = VK_NULL_HANDLE;
    VkFormat                 swapchainFormat  = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D               swapchainExtent  = {0, 0};
    std::vector<VkImage>     swapchainImages;
    std::vector<VkImageView> swapchainViews;
    uint32_t                 imageIndex       = 0;

    // Depth buffer
    VkImage                  depthImage       = VK_NULL_HANDLE;
    VkImageView              depthView        = VK_NULL_HANDLE;
    VmaAllocation            depthAlloc       = VK_NULL_HANDLE;
    VkFormat                 depthFormat      = VK_FORMAT_D32_SFLOAT;

    // Render pass & framebuffers
    VkRenderPass             renderPass       = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers;

    // Command pool & buffers (per frame-in-flight)
    VkCommandPool            commandPool      = VK_NULL_HANDLE;
    VkCommandBuffer          commandBuffers[VKB_MAX_FRAMES_IN_FLIGHT] = {};

    // Synchronization
    VkSemaphore              imageAvailable[VKB_MAX_FRAMES_IN_FLIGHT] = {};
    VkSemaphore              renderFinished[VKB_MAX_FRAMES_IN_FLIGHT] = {};
    VkFence                  inFlightFence[VKB_MAX_FRAMES_IN_FLIGHT]  = {};
    int                      currentFrame     = 0;

    // Per-frame vertex staging buffers
    VkBuffer                 stagingBuffer[VKB_MAX_FRAMES_IN_FLIGHT]  = {};
    VmaAllocation            stagingAlloc[VKB_MAX_FRAMES_IN_FLIGHT]   = {};
    void*                    stagingMapped[VKB_MAX_FRAMES_IN_FLIGHT]  = {};
    VkDeviceSize             stagingOffset    = 0; // current write offset this frame

    // State
    float                    clearColor[4]    = {0.0f, 0.0f, 0.0f, 1.0f};
    bool                     swapchainDirty   = false;
    bool                     framebufferResized = false;
    bool                     inRenderPass     = false;

    // Window
    GLFWwindow*              window           = nullptr;
};

// Global backend instance (defined in vk_backend.cpp).
extern VkBackend g_vk;

// Lifecycle
bool vkb_init(GLFWwindow* window);
void vkb_cleanup();

// Frame
bool vkb_begin_frame();         // Acquire image, begin command buffer + render pass
void vkb_end_frame();           // End render pass, submit, present
VkCommandBuffer vkb_cmd();      // Current frame's command buffer

// Swapchain
void vkb_recreate_swapchain();

// Helpers
VkShaderModule vkb_create_shader_module(const uint32_t* code, size_t size);
