/*
 * vk_backend.cpp — Vulkan bootstrap: instance, device, swapchain, render pass,
 *                  command buffers, frame sync, VMA allocator.
 */

// VMA needs aligned_alloc. Use C11 aligned_alloc directly (available in glibc 2.16+).
#define VMA_SYSTEM_ALIGNED_MALLOC(size, alignment) aligned_alloc((alignment), (((size) + (alignment) - 1) / (alignment)) * (alignment))
#define VMA_SYSTEM_ALIGNED_FREE(ptr) free(ptr)
#define VMA_VULKAN_VERSION 1000000
#define VMA_IMPLEMENTATION
#include "vk_backend.h"
#include "../SessionLog.h"

#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <set>

VkBackend g_vk;

// ─── Debug callback ────────────────────────────────────────────────────────
static VKAPI_ATTR VkBool32 VKAPI_CALL vkDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* /*user*/)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        SessionLog_Printf("[vk] %s\n", data->pMessage);
    return VK_FALSE;
}

// ─── Helper: find a supported depth format ─────────────────────────────────
static VkFormat findDepthFormat(VkPhysicalDevice pd)
{
    VkFormat candidates[] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
    for (auto fmt : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(pd, fmt, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            return fmt;
    }
    return VK_FORMAT_D32_SFLOAT;
}

// ─── Create swapchain + depth + framebuffers ───────────────────────────────
static bool createSwapchain()
{
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vk.physicalDevice, g_vk.surface, &caps);

    // Extent
    if (caps.currentExtent.width != UINT32_MAX)
    {
        g_vk.swapchainExtent = caps.currentExtent;
    }
    else
    {
        int w, h;
        glfwGetFramebufferSize(g_vk.window, &w, &h);
        g_vk.swapchainExtent.width  = std::max(caps.minImageExtent.width,  std::min(caps.maxImageExtent.width,  (uint32_t)w));
        g_vk.swapchainExtent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, (uint32_t)h));
    }
    if (g_vk.swapchainExtent.width == 0 || g_vk.swapchainExtent.height == 0)
        return false;

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    ci.surface          = g_vk.surface;
    ci.minImageCount    = imageCount;
    ci.imageFormat      = g_vk.swapchainFormat;
    ci.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    ci.imageExtent      = g_vk.swapchainExtent;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform     = caps.currentTransform;
    ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode      = VK_PRESENT_MODE_FIFO_KHR; // vsync
    ci.clipped          = VK_TRUE;
    ci.oldSwapchain     = g_vk.swapchain;

    VkSwapchainKHR newSwapchain;
    if (vkCreateSwapchainKHR(g_vk.device, &ci, nullptr, &newSwapchain) != VK_SUCCESS)
        return false;

    if (g_vk.swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(g_vk.device, g_vk.swapchain, nullptr);
    g_vk.swapchain = newSwapchain;

    // Get images
    uint32_t count = 0;
    vkGetSwapchainImagesKHR(g_vk.device, g_vk.swapchain, &count, nullptr);
    g_vk.swapchainImages.resize(count);
    vkGetSwapchainImagesKHR(g_vk.device, g_vk.swapchain, &count, g_vk.swapchainImages.data());

    // Image views
    for (auto v : g_vk.swapchainViews)
        vkDestroyImageView(g_vk.device, v, nullptr);
    g_vk.swapchainViews.resize(count);
    for (uint32_t i = 0; i < count; i++)
    {
        VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vi.image    = g_vk.swapchainImages[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format   = g_vk.swapchainFormat;
        vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCreateImageView(g_vk.device, &vi, nullptr, &g_vk.swapchainViews[i]);
    }

    // Depth buffer
    if (g_vk.depthView)  vkDestroyImageView(g_vk.device, g_vk.depthView, nullptr);
    if (g_vk.depthImage) vmaDestroyImage(g_vk.allocator, g_vk.depthImage, g_vk.depthAlloc);

    VkImageCreateInfo dci = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    dci.imageType   = VK_IMAGE_TYPE_2D;
    dci.format      = g_vk.depthFormat;
    dci.extent      = { g_vk.swapchainExtent.width, g_vk.swapchainExtent.height, 1 };
    dci.mipLevels   = 1;
    dci.arrayLayers = 1;
    dci.samples     = VK_SAMPLE_COUNT_1_BIT;
    dci.tiling      = VK_IMAGE_TILING_OPTIMAL;
    dci.usage       = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VmaAllocationCreateInfo dai = {};
    dai.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaCreateImage(g_vk.allocator, &dci, &dai, &g_vk.depthImage, &g_vk.depthAlloc, nullptr);

    VkImageViewCreateInfo dvi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    dvi.image    = g_vk.depthImage;
    dvi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    dvi.format   = g_vk.depthFormat;
    dvi.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
    vkCreateImageView(g_vk.device, &dvi, nullptr, &g_vk.depthView);

    // Framebuffers
    for (auto fb : g_vk.framebuffers)
        vkDestroyFramebuffer(g_vk.device, fb, nullptr);
    g_vk.framebuffers.resize(count);
    for (uint32_t i = 0; i < count; i++)
    {
        VkImageView attachments[] = { g_vk.swapchainViews[i], g_vk.depthView };
        VkFramebufferCreateInfo fi = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fi.renderPass      = g_vk.renderPass;
        fi.attachmentCount = 2;
        fi.pAttachments    = attachments;
        fi.width           = g_vk.swapchainExtent.width;
        fi.height          = g_vk.swapchainExtent.height;
        fi.layers          = 1;
        vkCreateFramebuffer(g_vk.device, &fi, nullptr, &g_vk.framebuffers[i]);
    }

    SessionLog_Printf("[vk] Swapchain %ux%u, %u images\n",
        g_vk.swapchainExtent.width, g_vk.swapchainExtent.height, count);
    return true;
}

// ─── vkb_init ──────────────────────────────────────────────────────────────
bool vkb_init(GLFWwindow* window)
{
    g_vk.window = window;

    // ── Instance ──
    VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName   = "Minecraft Console Edition";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "4JEngine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> extensions(glfwExts, glfwExts + glfwExtCount);

    // Try to enable validation layers in debug builds
    const char* validationLayer = "VK_LAYER_KHRONOS_validation";
    bool hasValidation = false;
#ifndef NDEBUG
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
    for (auto& l : layers)
    {
        if (strcmp(l.layerName, validationLayer) == 0)
        {
            hasValidation = true;
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            break;
        }
    }
#endif

    VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    ici.pApplicationInfo        = &appInfo;
    ici.enabledExtensionCount   = (uint32_t)extensions.size();
    ici.ppEnabledExtensionNames = extensions.data();
    if (hasValidation)
    {
        ici.enabledLayerCount   = 1;
        ici.ppEnabledLayerNames = &validationLayer;
    }

    if (vkCreateInstance(&ici, nullptr, &g_vk.instance) != VK_SUCCESS)
    {
        SessionLog_Printf("[vk] Failed to create VkInstance\n");
        return false;
    }

    // Debug messenger
    if (hasValidation)
    {
        auto createFunc = (PFN_vkCreateDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(g_vk.instance, "vkCreateDebugUtilsMessengerEXT");
        if (createFunc)
        {
            VkDebugUtilsMessengerCreateInfoEXT dci = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
            dci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            dci.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
            dci.pfnUserCallback = vkDebugCallback;
            createFunc(g_vk.instance, &dci, nullptr, &g_vk.debugMessenger);
        }
    }

    // ── Surface ──
    if (glfwCreateWindowSurface(g_vk.instance, window, nullptr, &g_vk.surface) != VK_SUCCESS)
    {
        SessionLog_Printf("[vk] Failed to create surface\n");
        return false;
    }

    // ── Physical device ──
    uint32_t pdCount = 0;
    vkEnumeratePhysicalDevices(g_vk.instance, &pdCount, nullptr);
    if (pdCount == 0) { SessionLog_Printf("[vk] No GPUs\n"); return false; }
    std::vector<VkPhysicalDevice> pds(pdCount);
    vkEnumeratePhysicalDevices(g_vk.instance, &pdCount, pds.data());

    // Pick first device with a graphics queue that supports present
    for (auto pd : pds)
    {
        uint32_t qfCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qfCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfs(qfCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qfCount, qfs.data());

        for (uint32_t i = 0; i < qfCount; i++)
        {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, g_vk.surface, &presentSupport);
            if ((qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport)
            {
                g_vk.physicalDevice = pd;
                g_vk.graphicsFamily = i;
                break;
            }
        }
        if (g_vk.physicalDevice) break;
    }
    if (!g_vk.physicalDevice) { SessionLog_Printf("[vk] No suitable GPU\n"); return false; }

    VkPhysicalDeviceProperties pdProps;
    vkGetPhysicalDeviceProperties(g_vk.physicalDevice, &pdProps);
    SessionLog_Printf("[vk] GPU: %s\n", pdProps.deviceName);

    g_vk.depthFormat = findDepthFormat(g_vk.physicalDevice);

    // ── Logical device ──
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    qci.queueFamilyIndex = g_vk.graphicsFamily;
    qci.queueCount       = 1;
    qci.pQueuePriorities = &queuePriority;

    const char* deviceExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    dci.queueCreateInfoCount    = 1;
    dci.pQueueCreateInfos       = &qci;
    dci.enabledExtensionCount   = 1;
    dci.ppEnabledExtensionNames = deviceExts;

    if (vkCreateDevice(g_vk.physicalDevice, &dci, nullptr, &g_vk.device) != VK_SUCCESS)
    {
        SessionLog_Printf("[vk] Failed to create device\n");
        return false;
    }
    vkGetDeviceQueue(g_vk.device, g_vk.graphicsFamily, 0, &g_vk.graphicsQueue);

    // ── VMA allocator ──
    VmaAllocatorCreateInfo aci = {};
    aci.physicalDevice = g_vk.physicalDevice;
    aci.device         = g_vk.device;
    aci.instance       = g_vk.instance;
    vmaCreateAllocator(&aci, &g_vk.allocator);

    // ── Render pass ──
    VkAttachmentDescription attachments[2] = {};
    // Color
    attachments[0].format         = g_vk.swapchainFormat;
    attachments[0].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    // Depth
    attachments[1].format         = g_vk.depthFormat;
    attachments[1].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depthRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep = {};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    rpci.attachmentCount = 2;
    rpci.pAttachments    = attachments;
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies   = &dep;

    if (vkCreateRenderPass(g_vk.device, &rpci, nullptr, &g_vk.renderPass) != VK_SUCCESS)
    {
        SessionLog_Printf("[vk] Failed to create render pass\n");
        return false;
    }

    // ── Swapchain + framebuffers ──
    if (!createSwapchain())
    {
        SessionLog_Printf("[vk] Failed to create swapchain\n");
        return false;
    }

    // ── Command pool + buffers ──
    VkCommandPoolCreateInfo cpci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    cpci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpci.queueFamilyIndex = g_vk.graphicsFamily;
    vkCreateCommandPool(g_vk.device, &cpci, nullptr, &g_vk.commandPool);

    VkCommandBufferAllocateInfo cbai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbai.commandPool        = g_vk.commandPool;
    cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = VKB_MAX_FRAMES_IN_FLIGHT;
    vkAllocateCommandBuffers(g_vk.device, &cbai, g_vk.commandBuffers);

    // ── Sync objects ──
    VkSemaphoreCreateInfo sci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fci     = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < VKB_MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkCreateSemaphore(g_vk.device, &sci, nullptr, &g_vk.imageAvailable[i]);
        vkCreateSemaphore(g_vk.device, &sci, nullptr, &g_vk.renderFinished[i]);
        vkCreateFence(g_vk.device, &fci, nullptr, &g_vk.inFlightFence[i]);
    }

    // ── Per-frame staging buffers ──
    for (int i = 0; i < VKB_MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bci.size  = VKB_STAGING_BUFFER_SIZE;
        bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        VmaAllocationCreateInfo bai = {};
        bai.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        bai.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VmaAllocationInfo info = {};
        vmaCreateBuffer(g_vk.allocator, &bci, &bai, &g_vk.stagingBuffer[i], &g_vk.stagingAlloc[i], &info);
        g_vk.stagingMapped[i] = info.pMappedData;
    }

    SessionLog_Printf("[vk] Vulkan backend initialized\n");
    return true;
}

// ─── vkb_cleanup ───────────────────────────────────────────────────────────
void vkb_cleanup()
{
    if (!g_vk.device) return;
    vkDeviceWaitIdle(g_vk.device);

    for (int i = 0; i < VKB_MAX_FRAMES_IN_FLIGHT; i++)
    {
        vmaDestroyBuffer(g_vk.allocator, g_vk.stagingBuffer[i], g_vk.stagingAlloc[i]);
        vkDestroySemaphore(g_vk.device, g_vk.imageAvailable[i], nullptr);
        vkDestroySemaphore(g_vk.device, g_vk.renderFinished[i], nullptr);
        vkDestroyFence(g_vk.device, g_vk.inFlightFence[i], nullptr);
    }

    vkDestroyCommandPool(g_vk.device, g_vk.commandPool, nullptr);

    for (auto fb : g_vk.framebuffers)
        vkDestroyFramebuffer(g_vk.device, fb, nullptr);

    vkDestroyImageView(g_vk.device, g_vk.depthView, nullptr);
    vmaDestroyImage(g_vk.allocator, g_vk.depthImage, g_vk.depthAlloc);

    for (auto v : g_vk.swapchainViews)
        vkDestroyImageView(g_vk.device, v, nullptr);

    vkDestroySwapchainKHR(g_vk.device, g_vk.swapchain, nullptr);
    vkDestroyRenderPass(g_vk.device, g_vk.renderPass, nullptr);

    vmaDestroyAllocator(g_vk.allocator);
    vkDestroyDevice(g_vk.device, nullptr);
    vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, nullptr);

    if (g_vk.debugMessenger)
    {
        auto destroyFunc = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(g_vk.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroyFunc) destroyFunc(g_vk.instance, g_vk.debugMessenger, nullptr);
    }

    vkDestroyInstance(g_vk.instance, nullptr);
    g_vk = {};
    SessionLog_Printf("[vk] Vulkan backend cleaned up\n");
}

// ─── vkb_recreate_swapchain ────────────────────────────────────────────────
void vkb_recreate_swapchain()
{
    int w = 0, h = 0;
    glfwGetFramebufferSize(g_vk.window, &w, &h);
    while (w == 0 || h == 0)
    {
        glfwGetFramebufferSize(g_vk.window, &w, &h);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(g_vk.device);
    createSwapchain();
    g_vk.swapchainDirty = false;
}

// ─── vkb_begin_frame ───────────────────────────────────────────────────────
bool vkb_begin_frame()
{
    int f = g_vk.currentFrame;

    vkWaitForFences(g_vk.device, 1, &g_vk.inFlightFence[f], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(g_vk.device, g_vk.swapchain, UINT64_MAX,
        g_vk.imageAvailable[f], VK_NULL_HANDLE, &g_vk.imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || g_vk.swapchainDirty)
    {
        vkb_recreate_swapchain();
        return false; // skip this frame
    }

    vkResetFences(g_vk.device, 1, &g_vk.inFlightFence[f]);

    // Reset staging offset
    g_vk.stagingOffset = 0;

    // Begin command buffer
    VkCommandBuffer cmd = g_vk.commandBuffers[f];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);

    // Begin render pass
    VkClearValue clearValues[2] = {};
    clearValues[0].color = {{ g_vk.clearColor[0], g_vk.clearColor[1], g_vk.clearColor[2], g_vk.clearColor[3] }};
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo rpbi = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    rpbi.renderPass        = g_vk.renderPass;
    rpbi.framebuffer       = g_vk.framebuffers[g_vk.imageIndex];
    rpbi.renderArea.offset = { 0, 0 };
    rpbi.renderArea.extent = g_vk.swapchainExtent;
    rpbi.clearValueCount   = 2;
    rpbi.pClearValues      = clearValues;

    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    // Set default viewport and scissor
    VkViewport vp = { 0, 0, (float)g_vk.swapchainExtent.width, (float)g_vk.swapchainExtent.height, 0.0f, 1.0f };
    vkCmdSetViewport(cmd, 0, 1, &vp);
    VkRect2D scissor = { {0, 0}, g_vk.swapchainExtent };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    g_vk.inRenderPass = true;
    return true;
}

// ─── vkb_end_frame ─────────────────────────────────────────────────────────
void vkb_end_frame()
{
    int f = g_vk.currentFrame;
    VkCommandBuffer cmd = g_vk.commandBuffers[f];

    if (g_vk.inRenderPass)
    {
        vkCmdEndRenderPass(cmd);
        g_vk.inRenderPass = false;
    }

    vkEndCommandBuffer(cmd);

    // Submit
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &g_vk.imageAvailable[f];
    si.pWaitDstStageMask    = &waitStage;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &g_vk.renderFinished[f];

    vkQueueSubmit(g_vk.graphicsQueue, 1, &si, g_vk.inFlightFence[f]);

    // Present
    VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &g_vk.renderFinished[f];
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &g_vk.swapchain;
    pi.pImageIndices      = &g_vk.imageIndex;

    VkResult result = vkQueuePresentKHR(g_vk.graphicsQueue, &pi);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        g_vk.swapchainDirty = true;

    g_vk.currentFrame = (f + 1) % VKB_MAX_FRAMES_IN_FLIGHT;
}

// ─── vkb_cmd ───────────────────────────────────────────────────────────────
VkCommandBuffer vkb_cmd()
{
    return g_vk.commandBuffers[g_vk.currentFrame];
}

// ─── vkb_create_shader_module ──────────────────────────────────────────────
VkShaderModule vkb_create_shader_module(const uint32_t* code, size_t size)
{
    VkShaderModuleCreateInfo ci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    ci.codeSize = size;
    ci.pCode    = code;
    VkShaderModule mod;
    if (vkCreateShaderModule(g_vk.device, &ci, nullptr, &mod) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return mod;
}
