#ifndef VULKAN_CONTEXT_H
#define VULKAN_CONTEXT_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

enum rendererStatus { VULKAN_CONTEXT_FAILURE, VULKAN_CONTEXT_SUCCESS };

struct QueueFamilyIndices {
    int32_t graphics;
    int32_t present;
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *formats;
    uint32_t formatsCount;
    VkPresentModeKHR *presentModes;
    uint32_t presentModesCount;
};

int initializeVulkanContext(GLFWwindow *window);

#endif