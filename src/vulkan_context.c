#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vulkan_context.h"

#include "util.h"

static uint32_t isDeviceSuitable(VkPhysicalDevice device);
static struct QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);
static uint32_t checkDeviceExtensionSupport(VkPhysicalDevice device);
static struct SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const VkSurfaceFormatKHR *availableFormats, uint32_t formatsCount);
static VkPresentModeKHR chooseSwapPresentMode(const VkPresentModeKHR *availablePresentModes, uint32_t presentModesCount);
static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR capabilities, GLFWwindow *window);
static VkSwapchainKHR createSwapChain(VkPhysicalDevice physicalDevice, VkDevice device, GLFWwindow *window, 
                                      struct QueueFamilyIndices queueFamilyIndices, const uint32_t *indices, uint32_t indexCount);
static VkResult createRenderPass(VkDevice device);
static VkResult createGraphicsPipeline(VkDevice device);
static VkFramebuffer *createFramebuffers(VkDevice device, VkImageView *swapChainImageViews, uint32_t imageCount);
static VkShaderModule createShaderModule(VkDevice device, const uint8_t *code, uint32_t size);

#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof((arr)[0]))

static const char *validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
static const char *deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
static VkSurfaceKHR surface;
static VkSurfaceFormatKHR swapChainImageFormat;
static VkPresentModeKHR swapChainPresentMode;
static VkExtent2D swapChainExtent;
static VkRenderPass renderPass;
static VkPipelineLayout pipelineLayout;
static VkPipeline graphicsPipeline;

int initializeVulkanContext(GLFWwindow *window) {
    // Specify information necessary to create a Vulkan instance
    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    // Specify required instance extensions
    // GLFW has a function that returns the extensions it needs.
    uint32_t extensionCount = 0;
    const char **glfwExtensions;
    if ((glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount))) {
        instanceCreateInfo.enabledExtensionCount = extensionCount;
        instanceCreateInfo.ppEnabledExtensionNames = glfwExtensions;
    } else {
        fprintf(stderr, "Failed to satisfy GLFW's extension requirements\n");
        return VULKAN_CONTEXT_FAILURE;
    }

    // Specify desired validation layers (debug builds only)
#ifndef NDEBUG
    instanceCreateInfo.enabledLayerCount = ARRAY_LENGTH(validationLayers);
    instanceCreateInfo.ppEnabledLayerNames = validationLayers;
#else
    instanceCreateInfo.enabledLayerCount = 0;
#endif

    // Create a Vulkan instance using the information declared above
    VkInstance instance;
    if (vkCreateInstance(&instanceCreateInfo, NULL, &instance) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan instance\n");
        return VULKAN_CONTEXT_FAILURE;
    }

    // Create a window surface
    if (glfwCreateWindowSurface(instance, window, NULL, &surface) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create window surface\n");
        return VULKAN_CONTEXT_FAILURE;
    }

    // Pick a suitable rendering device
    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, NULL);

    VkPhysicalDevice physicalDevices[physicalDeviceCount];
    if (physicalDeviceCount == 0 || 
        vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices) != VK_SUCCESS)
    {
        fprintf(stderr, "Failed to detect physical devices\n");
        return VULKAN_CONTEXT_FAILURE;
    }

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    for (size_t i = 0; i < physicalDeviceCount; ++i) {
        if (isDeviceSuitable(physicalDevices[i])) {
            physicalDevice = physicalDevices[i];
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        fprintf(stderr, "Failed to find a suitable rendering device\n");
        return VULKAN_CONTEXT_FAILURE;
    }

    // Specify which and how many queues we want
    struct QueueFamilyIndices queueFamilyIndices = getQueueFamilies(physicalDevice);

    uint32_t indices[] = {
        (uint32_t) queueFamilyIndices.graphics,
        (uint32_t) queueFamilyIndices.present
    };

    // Specify information necessary to create the device queues
    VkDeviceQueueCreateInfo queueCreateInfos[ARRAY_LENGTH(indices)];
    for (size_t i = 0; i < ARRAY_LENGTH(indices); ++i) {
        queueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[i].queueFamilyIndex = indices[i];
        queueCreateInfos[i].queueCount = 1;
        float queuePriority = 1.0f;
        queueCreateInfos[i].pQueuePriorities = &queuePriority;
    }

    // Specify which physical device features we will use
    VkPhysicalDeviceFeatures deviceFeatures = {0};

    // Specify information necessary to create a logical device
    VkDeviceCreateInfo deviceCreateInfo = {0};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;
    deviceCreateInfo.queueCreateInfoCount = ARRAY_LENGTH(indices);
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    deviceCreateInfo.enabledExtensionCount = ARRAY_LENGTH(deviceExtensions);
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

    // Create a logical device using the information declared above
    // Device queues are automatically created here as well
    VkDevice device;
    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create the logical device\n");
        return VULKAN_CONTEXT_FAILURE;
    }

    // Get a handle for each queue
    VkQueue graphicsQueue;
    vkGetDeviceQueue(device, queueFamilyIndices.graphics, 0, &graphicsQueue);

    VkQueue presentQueue;
    vkGetDeviceQueue(device, queueFamilyIndices.present, 0, &presentQueue);

    // Create a swap chain
    VkSwapchainKHR swapChain = createSwapChain(
        physicalDevice, device, window, 
        queueFamilyIndices, indices, ARRAY_LENGTH(indices)
    );

    if (!swapChain) {
        fprintf(stderr, "Failed to create swap chain\n");
        return VULKAN_CONTEXT_FAILURE;
    }

    // Get handles to the swap chain images
    uint32_t swapChainImageCount = 0;
    vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCount, NULL);
    VkImage swapChainImages[swapChainImageCount];
    vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCount, swapChainImages);

    // For each image in the swap chain, specify information necessary
    // to create an image view and then create it.
    VkImageView swapChainImageViews[swapChainImageCount];
    for (size_t i = 0; i < swapChainImageCount; ++i) {
        VkImageViewCreateInfo imageViewCreateInfo = {0};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = swapChainImages[i];
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = swapChainImageFormat.format;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(device, &imageViewCreateInfo, NULL, swapChainImageViews) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create swap chain image views\n");
            return VULKAN_CONTEXT_FAILURE;
        }
    }

    createRenderPass(device);
    createGraphicsPipeline(device);

    return VULKAN_CONTEXT_SUCCESS;
}

static uint32_t isDeviceSuitable(VkPhysicalDevice device) {
    /*VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevices[i], &deviceFeatures);*/

    struct QueueFamilyIndices indices = getQueueFamilies(device);
    uint32_t validIndices = (indices.graphics >= 0) && (indices.present >= 0);

    uint32_t extensionsSupported = checkDeviceExtensionSupport(device);

    uint32_t swapChainAdequate = 0;
    if (extensionsSupported) {
        struct SwapChainSupportDetails swapChainDetails = querySwapChainSupport(device);
        swapChainAdequate = sizeof(swapChainDetails.formats) && sizeof(swapChainDetails.presentModes);
    }

    return validIndices && extensionsSupported && swapChainAdequate;
}

static struct QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device) {
    // Query amount of queue families
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);

    // Store properties of the available queue families
    VkQueueFamilyProperties queueFamiliesProperties[queueFamilyCount];
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamiliesProperties);

    // Return information about what types of queues are available
    struct QueueFamilyIndices indices = {-1, -1};
    for (size_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamiliesProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphics = i;

        VkBool32 presentSupport = 0;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) indices.present = i;
    }

    return indices;
}

static uint32_t checkDeviceExtensionSupport(VkPhysicalDevice device) {
    // Query amount of available extensions
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);

    // Store properties of the available extensions
    VkExtensionProperties availableExtensions[extensionCount];
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, availableExtensions);

    // Make sure that deviceExtensions is a subset of availableExtensions
    uint32_t extensionsSatisfied = 1;
    for (size_t i = 0; i < ARRAY_LENGTH(deviceExtensions); ++i) {
        uint32_t extensionFound = 0;
        for (size_t j = 0; j < extensionCount; ++j) {
            if (!strcmp(deviceExtensions[i], availableExtensions[j].extensionName)) {
                extensionFound = 1;
                break;
            }
        }
        if (!extensionFound) {
            extensionsSatisfied = 0;
            break;
        }
    }

    return extensionsSatisfied;
}

static struct SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
    struct SwapChainSupportDetails details = {0};

    // Surface capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    // Surface format
    uint32_t formatsCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatsCount, NULL);
    details.formatsCount = formatsCount;
    VkSurfaceFormatKHR formats[sizeof(VkSurfaceFormatKHR) * formatsCount];
    details.formats = formats;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatsCount, details.formats);

    // Present modes
    uint32_t presentModesCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModesCount, NULL);
    details.presentModesCount = presentModesCount;
    VkPresentModeKHR presentModes[sizeof(VkPresentModeKHR) * presentModesCount];
    details.presentModes = presentModes;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModesCount, details.presentModes);

    return details;
}

static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const VkSurfaceFormatKHR *availableFormats, uint32_t formatsCount) {
    for (size_t i = 0; i < formatsCount; ++i) {
        if (availableFormats[i].format == VK_FORMAT_B8G8R8_SRGB && 
            availableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return availableFormats[i];
    }

    return availableFormats[0];
}

static VkPresentModeKHR chooseSwapPresentMode(const VkPresentModeKHR *availablePresentModes, uint32_t presentModesCount) {
    for (size_t i = 0; i < presentModesCount; ++i) {
        if (availablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            return availablePresentModes[i];
    }
    
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D chooseSwapExtent(VkSurfaceCapabilitiesKHR capabilities, GLFWwindow *window) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        int signedWidth, signedHeight;
        glfwGetFramebufferSize(window, &signedWidth, &signedHeight);

        uint32_t width = (uint32_t) signedWidth;
        uint32_t height = (uint32_t) signedHeight;
        VkExtent2D actualExtent = {width, height};
        
        // Clamp width and height values
        actualExtent.width = width < capabilities.minImageExtent.width ? 
                             capabilities.minImageExtent.width : width;
        actualExtent.width = width > capabilities.maxImageExtent.width ? 
                             capabilities.maxImageExtent.width : width;
        actualExtent.height = height < capabilities.minImageExtent.height ? 
                              capabilities.minImageExtent.height : height;
        actualExtent.height = height > capabilities.maxImageExtent.height ? 
                              capabilities.maxImageExtent.height : height;
        
        return actualExtent;
    }
}

static VkSwapchainKHR createSwapChain(VkPhysicalDevice physicalDevice,
                                      VkDevice device, 
                                      GLFWwindow *window, 
                                      struct QueueFamilyIndices queueFamilyIndices,
                                      const uint32_t *indices,
                                      uint32_t indexCount)
{
    // Configure information required to create a swap chain
    struct SwapChainSupportDetails swapChainDetails = querySwapChainSupport(physicalDevice);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(
        swapChainDetails.formats, swapChainDetails.formatsCount);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(
        swapChainDetails.presentModes, swapChainDetails.presentModesCount);
    VkExtent2D extent = chooseSwapExtent(swapChainDetails.capabilities, window);

    // Store selected format, present mode and extent in global variables
    swapChainImageFormat = surfaceFormat;
    swapChainPresentMode = presentMode;
    swapChainExtent = extent;

    uint32_t imageCount = swapChainDetails.capabilities.minImageCount + 1;
    uint32_t maxImageCount = swapChainDetails.capabilities.maxImageCount;
    if (maxImageCount > 0 && imageCount > maxImageCount) imageCount = maxImageCount;

    // Specify information necessary to create a swap chain
    VkSwapchainCreateInfoKHR createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Set image sharing mode based on whether the two queues share the same index
    if (queueFamilyIndices.graphics != queueFamilyIndices.present) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = indexCount;
        createInfo.pQueueFamilyIndices = indices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainDetails.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    // Create swap chain
    VkSwapchainKHR swapChain;
    if (vkCreateSwapchainKHR(device, &createInfo, NULL, &swapChain) != VK_SUCCESS)
        return NULL;

    return swapChain;
}

static VkResult createRenderPass(VkDevice device) {
    VkAttachmentDescription colorAttachment = {0};
    colorAttachment.format = swapChainImageFormat.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {0};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassCreateInfo = {0};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &colorAttachment;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(device, &renderPassCreateInfo, NULL, &renderPass) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;
    


    return VK_SUCCESS;
}

static VkResult createGraphicsPipeline(VkDevice device) {
    uint32_t vertShaderSize = 0;
    uint8_t *vertShaderCode = readBinaryFile("shaders/vert.spv", &vertShaderSize);
    uint32_t fragShaderSize = 0;
    uint8_t *fragShaderCode = readBinaryFile("shaders/frag.spv", &fragShaderSize);

    VkShaderModule vertShaderModule = createShaderModule(device, vertShaderCode, vertShaderSize);
    VkShaderModule fragShaderModule = createShaderModule(device, fragShaderCode, fragShaderSize);

    free(vertShaderCode);
    free(fragShaderCode);

    // Specify information regarding the shader stages in the pipeline
    VkPipelineShaderStageCreateInfo vertStageCreateInfo = {0};
    vertStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageCreateInfo.module = vertShaderModule;
    vertStageCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragStageCreateInfo = {0};
    fragStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageCreateInfo.module = fragShaderModule;
    fragStageCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertStageCreateInfo, fragStageCreateInfo};
    
    // Specify information regarding vertex input attributes
    VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {0};
    vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
    vertexInputCreateInfo.pVertexBindingDescriptions = NULL;
    vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
    vertexInputCreateInfo.pVertexAttributeDescriptions = NULL;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {0};
    inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {0};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) swapChainExtent.width;
    viewport.height = (float) swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkOffset2D scissorOffset = {0, 0};
    VkRect2D scissor = {0};
    scissor.offset = scissorOffset;
    scissor.extent = swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState = {0};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo = {0};
    rasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationCreateInfo.lineWidth = 1.0f;
    rasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizationCreateInfo.depthBiasEnable = VK_FALSE;
    rasterizationCreateInfo.depthBiasConstantFactor = 0.0f;
    rasterizationCreateInfo.depthBiasClamp = 0.0f;
    rasterizationCreateInfo.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = {0};
    multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleCreateInfo.minSampleShading = 1.0f;
    multisampleCreateInfo.pSampleMask = NULL;
    multisampleCreateInfo.alphaToCoverageEnable = VK_FALSE;
    multisampleCreateInfo.alphaToOneEnable = VK_FALSE;

    // TODO(Hansi): Depth and stencil testing

    // These settings implement simple alpha blending
    // However, both of these structs are currently disabled
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {0};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending = {0};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // Dynamic state enables changing some pipeline options on the fly
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_LINE_WIDTH
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {0};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = ARRAY_LENGTH(dynamicStates);
    dynamicState.pDynamicStates = dynamicStates;

    // Pipeline layout is related to uniform variables
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {0};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 0;
    pipelineLayoutCreateInfo.pSetLayouts = NULL;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = NULL;

    if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, &pipelineLayout) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {0};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = ARRAY_LENGTH(shaderStages);
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterizationCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
    pipelineCreateInfo.pDepthStencilState = NULL;
    pipelineCreateInfo.pColorBlendState = &colorBlending;
    pipelineCreateInfo.pDynamicState = NULL;
    pipelineCreateInfo.layout = pipelineLayout;
    pipelineCreateInfo.renderPass = renderPass;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &graphicsPipeline) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;

    vkDestroyShaderModule(device, vertShaderModule, NULL);
    vkDestroyShaderModule(device, fragShaderModule, NULL);

    return VK_SUCCESS;
}

static VkFramebuffer *createFramebuffers(VkDevice device, VkImageView *swapChainImageViews, uint32_t imageCount) {
    VkFramebuffer *framebuffers = (VkFramebuffer *) malloc(sizeof(VkFramebuffer) * imageCount);

    for (size_t i = 0; i < imageCount; ++i) {
        VkImageView attachments[] = {
            swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferCreateInfo = {0};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = renderPass;
        framebufferCreateInfo.attachmentCount = 1;
        framebufferCreateInfo.pAttachments = attachments;
        framebufferCreateInfo.width = swapChainExtent.width;
        framebufferCreateInfo.height = swapChainExtent.height;
        framebufferCreateInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferCreateInfo, NULL, &framebuffers[i]) != VK_SUCCESS)
            return NULL;
    }

    return framebuffers;
}

static VkShaderModule createShaderModule(VkDevice device, const uint8_t *code, uint32_t size) {
    // Specify information necessary to create a shader module
    VkShaderModuleCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size;
    // TODO(Hansi): Check if this actually works
    createInfo.pCode = (const uint32_t *) code;

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, NULL, &shaderModule) != VK_SUCCESS)
        return NULL;

    return shaderModule;
}