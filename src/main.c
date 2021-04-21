#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdio.h>

#include "vulkan_context.h"

const uint32_t windowWidth = 800;
const uint32_t windowHeight = 600;

int main() {
    // Initialize GLFW and create a window
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow *window = glfwCreateWindow(windowWidth, windowHeight, "Vulkan", NULL, NULL);

    if (initializeVulkanContext(window) != VULKAN_CONTEXT_SUCCESS) {
        fprintf(stderr, "Failed to initialize renderer\n");
        return -1;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    return 0;
}