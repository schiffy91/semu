// vulkan-present: presents a known picture through a Vulkan library with no window, so Semu's
// Vulkan composition (the Linux layer, the macOS stand-in for MoltenVK) can be checked offscreen.
// A headless swapchain (VK_EXT_headless_surface) receives a test card every frame: a color-bar
// field with a white frame one pixel inside the picture and a black diagonal from its top left.
// Composition, when Semu's library is in the path, runs at each present; SEMU_RENDER_CAPTURE_FRAME
// makes the renderer save the composed frame.
// build: cc -O2 vulkan-present.c -I<vulkan-headers>/include -ldl -o vulkan-present
// usage: vulkan-present LIBRARY WIDTH HEIGHT FRAMES   (LIBRARY: libvulkan.so.1, or Semu's stand-in)
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define CHECK(call) do { VkResult result_ = (call); if (result_ < 0) { fprintf(stderr, "vulkan-present: %s failed: %d\n", #call, result_); return 1; } } while (0)

static PFN_vkGetInstanceProcAddr getInstanceProc;

static void *instanceProc(VkInstance instance, const char *name) {
    void *function = (void *)getInstanceProc(instance, name);
    if (!function) { fprintf(stderr, "vulkan-present: no %s\n", name); exit(1); }
    return function;
}

static uint32_t testCard(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {  // RGBA bytes little-endian
    static const uint32_t bars[7] = { 0xFFC0C0C0u, 0xFF00C0C0u, 0xFFC0C000u, 0xFF00C000u, 0xFFC000C0u, 0xFF0000C0u, 0xFFC00000u };
    if (x == 1 || y == 1 || x == width - 2 || y == height - 2) return 0xFFFFFFFFu;
    if (x * height / width == y) return 0xFF000000u;
    return bars[x * 7 / width];
}

int main(int argc, char **argv) {
    if (argc != 5) { fprintf(stderr, "usage: vulkan-present LIBRARY WIDTH HEIGHT FRAMES\n"); return 2; }
    uint32_t width = (uint32_t)atoi(argv[2]), height = (uint32_t)atoi(argv[3]), frames = (uint32_t)atoi(argv[4]);
    void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) { fprintf(stderr, "vulkan-present: %s\n", dlerror()); return 1; }
    getInstanceProc = (PFN_vkGetInstanceProcAddr)dlsym(library, "vkGetInstanceProcAddr");
    if (!getInstanceProc) { fprintf(stderr, "vulkan-present: no vkGetInstanceProcAddr in %s\n", argv[1]); return 1; }

    const char *instanceExtensions[] = { "VK_KHR_surface", "VK_EXT_headless_surface" };
    VkApplicationInfo application = { VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "vulkan-present", 1, NULL, 0, VK_API_VERSION_1_1 };
    VkInstanceCreateInfo instanceInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &application, 0, NULL, 2, instanceExtensions };
    VkInstance instance;
    CHECK(((PFN_vkCreateInstance)instanceProc(NULL, "vkCreateInstance"))(&instanceInfo, NULL, &instance));

    VkHeadlessSurfaceCreateInfoEXT surfaceInfo = { VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT, NULL, 0 };
    VkSurfaceKHR surface;
    CHECK(((PFN_vkCreateHeadlessSurfaceEXT)instanceProc(instance, "vkCreateHeadlessSurfaceEXT"))(instance, &surfaceInfo, NULL, &surface));
    uint32_t count = 1;
    VkPhysicalDevice physical;
    CHECK(((PFN_vkEnumeratePhysicalDevices)instanceProc(instance, "vkEnumeratePhysicalDevices"))(instance, &count, &physical));
    VkPhysicalDeviceMemoryProperties memory;
    ((PFN_vkGetPhysicalDeviceMemoryProperties)instanceProc(instance, "vkGetPhysicalDeviceMemoryProperties"))(physical, &memory);

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0, 0, 1, &priority };
    const char *deviceExtensions[] = { "VK_KHR_swapchain" };
    VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL, 0, 1, &queueInfo, 0, NULL, 1, deviceExtensions, NULL };
    VkDevice device;
    CHECK(((PFN_vkCreateDevice)instanceProc(instance, "vkCreateDevice"))(physical, &deviceInfo, NULL, &device));
    PFN_vkGetDeviceProcAddr getDeviceProc = (PFN_vkGetDeviceProcAddr)instanceProc(instance, "vkGetDeviceProcAddr");
    PFN_vkGetDeviceQueue getQueue = (PFN_vkGetDeviceQueue)getDeviceProc(device, "vkGetDeviceQueue");
    PFN_vkCreateSwapchainKHR createSwapchain = (PFN_vkCreateSwapchainKHR)getDeviceProc(device, "vkCreateSwapchainKHR");
    PFN_vkGetSwapchainImagesKHR swapchainImages = (PFN_vkGetSwapchainImagesKHR)getDeviceProc(device, "vkGetSwapchainImagesKHR");
    PFN_vkAcquireNextImageKHR acquire = (PFN_vkAcquireNextImageKHR)getDeviceProc(device, "vkAcquireNextImageKHR");
    PFN_vkQueuePresentKHR present = (PFN_vkQueuePresentKHR)getDeviceProc(device, "vkQueuePresentKHR");
    PFN_vkCreateBuffer createBuffer = (PFN_vkCreateBuffer)getDeviceProc(device, "vkCreateBuffer");
    PFN_vkGetBufferMemoryRequirements bufferRequirements = (PFN_vkGetBufferMemoryRequirements)getDeviceProc(device, "vkGetBufferMemoryRequirements");
    PFN_vkAllocateMemory allocate = (PFN_vkAllocateMemory)getDeviceProc(device, "vkAllocateMemory");
    PFN_vkBindBufferMemory bindBuffer = (PFN_vkBindBufferMemory)getDeviceProc(device, "vkBindBufferMemory");
    PFN_vkMapMemory map = (PFN_vkMapMemory)getDeviceProc(device, "vkMapMemory");
    PFN_vkCreateCommandPool createPool = (PFN_vkCreateCommandPool)getDeviceProc(device, "vkCreateCommandPool");
    PFN_vkAllocateCommandBuffers allocateCommands = (PFN_vkAllocateCommandBuffers)getDeviceProc(device, "vkAllocateCommandBuffers");
    PFN_vkBeginCommandBuffer begin = (PFN_vkBeginCommandBuffer)getDeviceProc(device, "vkBeginCommandBuffer");
    PFN_vkEndCommandBuffer end = (PFN_vkEndCommandBuffer)getDeviceProc(device, "vkEndCommandBuffer");
    PFN_vkCmdPipelineBarrier barrier = (PFN_vkCmdPipelineBarrier)getDeviceProc(device, "vkCmdPipelineBarrier");
    PFN_vkCmdCopyBufferToImage copy = (PFN_vkCmdCopyBufferToImage)getDeviceProc(device, "vkCmdCopyBufferToImage");
    PFN_vkQueueSubmit submit = (PFN_vkQueueSubmit)getDeviceProc(device, "vkQueueSubmit");
    PFN_vkCreateFence createFence = (PFN_vkCreateFence)getDeviceProc(device, "vkCreateFence");
    PFN_vkWaitForFences waitFences = (PFN_vkWaitForFences)getDeviceProc(device, "vkWaitForFences");
    PFN_vkResetFences resetFences = (PFN_vkResetFences)getDeviceProc(device, "vkResetFences");
    PFN_vkCreateSemaphore createSemaphore = (PFN_vkCreateSemaphore)getDeviceProc(device, "vkCreateSemaphore");
    PFN_vkResetCommandBuffer resetCommands = (PFN_vkResetCommandBuffer)getDeviceProc(device, "vkResetCommandBuffer");

    VkQueue queue;
    getQueue(device, 0, 0, &queue);
    VkSwapchainCreateInfoKHR swapchainInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, NULL, 0, surface, 3, VK_FORMAT_B8G8R8A8_UNORM,
        VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, { width, height }, 1, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_SHARING_MODE_EXCLUSIVE, 0, NULL, VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR, VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_PRESENT_MODE_FIFO_KHR, VK_TRUE, VK_NULL_HANDLE };
    VkSwapchainKHR swapchain;
    CHECK(createSwapchain(device, &swapchainInfo, NULL, &swapchain));
    uint32_t imageCount = 8;
    VkImage images[8];
    CHECK(swapchainImages(device, swapchain, &imageCount, images));

    VkDeviceSize size = (VkDeviceSize)width * height * 4;
    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL };
    VkBuffer buffer;
    CHECK(createBuffer(device, &bufferInfo, NULL, &buffer));
    VkMemoryRequirements requirements;
    bufferRequirements(device, buffer, &requirements);
    uint32_t type = 0;
    for (uint32_t index = 0; index < memory.memoryTypeCount; index++) {
        if ((requirements.memoryTypeBits & (1u << index)) && (memory.memoryTypes[index].propertyFlags & 6u) == 6u) { type = index; break; }  // HOST_VISIBLE | HOST_COHERENT
    }
    VkMemoryAllocateInfo allocation = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, requirements.size, type };
    VkDeviceMemory bufferMemory;
    CHECK(allocate(device, &allocation, NULL, &bufferMemory));
    CHECK(bindBuffer(device, buffer, bufferMemory, 0));
    uint32_t *pixels;
    CHECK(map(device, bufferMemory, 0, size, 0, (void **)&pixels));
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t rgba = testCard(x, y, width, height);
            pixels[y * width + x] = (rgba & 0xFF00FF00u) | ((rgba & 0xFFu) << 16) | ((rgba >> 16) & 0xFFu);  // BGRA bytes
        }
    }

    VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, 0 };
    VkCommandPool pool;
    CHECK(createPool(device, &poolInfo, NULL, &pool));
    VkCommandBufferAllocateInfo commandInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
    VkCommandBuffer command;
    CHECK(allocateCommands(device, &commandInfo, &command));
    VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0 };
    VkFence acquired, done;
    CHECK(createFence(device, &fenceInfo, NULL, &acquired));
    CHECK(createFence(device, &fenceInfo, NULL, &done));
    VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, NULL, 0 };
    VkSemaphore rendered;
    CHECK(createSemaphore(device, &semaphoreInfo, NULL, &rendered));

    for (uint32_t frame = 0; frame < frames; frame++) {
        uint32_t index;
        CHECK(acquire(device, swapchain, UINT64_MAX, VK_NULL_HANDLE, acquired, &index));
        CHECK(waitFences(device, 1, &acquired, VK_TRUE, UINT64_MAX));
        CHECK(resetFences(device, 1, &acquired));
        resetCommands(command, 0);
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, NULL };
        CHECK(begin(command, &beginInfo));
        VkImageMemoryBarrier toCopy = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, images[index], { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
        barrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &toCopy);
        VkBufferImageCopy region = { 0, 0, 0, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { 0, 0, 0 }, { width, height, 1 } };
        copy(command, buffer, images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        VkImageMemoryBarrier toPresent = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL, VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, images[index], { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
        barrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &toPresent);
        CHECK(end(command));
        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0, NULL, NULL, 1, &command, 1, &rendered };
        CHECK(submit(queue, 1, &submitInfo, done));
        VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, NULL, 1, &rendered, 1, &swapchain, &index, NULL };
        CHECK(present(queue, &presentInfo));
        CHECK(waitFences(device, 1, &done, VK_TRUE, UINT64_MAX));
        CHECK(resetFences(device, 1, &done));
    }
    printf("vulkan-present: %u frames presented at %ux%u\n", frames, width, height);
    return 0;
}
