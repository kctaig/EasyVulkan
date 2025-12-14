#pragma once

#include "EasyVKStart.h"

namespace vulkan {
class graphicsBase {
   private:
    static graphicsBase singleton;

    uint32_t apiVersion = VK_API_VERSION_1_0;
    VkInstance instance{};
    std::vector<const char*> instanceLayers{};
    std::vector<const char*> instanceExtensions{};

    VkPhysicalDevice physicalDevice{};
    VkPhysicalDeviceProperties physicalDeviceProperties{};
    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties{};
    std::vector<VkPhysicalDevice> availablePhysicalDevices{};

    VkDevice device{};
    std::vector<const char*> deviceExtensions{};
    uint32_t queueFamilyIndex_graphics = VK_QUEUE_FAMILY_IGNORED;
    uint32_t queueFamilyIndex_presentation = VK_QUEUE_FAMILY_IGNORED;
    uint32_t queueFamilyIndex_compute = VK_QUEUE_FAMILY_IGNORED;
    VkQueue queue_graphics{};
    VkQueue queue_presentation{};
    VkQueue queue_compute{};

    VkSurfaceKHR surface{};
    std::vector<VkSurfaceFormatKHR> availableSurfaceFormats{};

    VkDebugUtilsMessengerEXT debugMessenger{};

    VkSwapchainKHR swapchain{};
    std::vector<VkImage> swapchainImages{};
    std::vector<VkImageView> swapchainImageViews{};
    VkSwapchainCreateInfoKHR swapchainCreateInfo{};

    /******* private function *********/

    graphicsBase() = default;
    graphicsBase(graphicsBase&&) = delete;
    ~graphicsBase() {}

    // 添加实例层或扩展
    static void AddLayerOrExtension(std::vector<const char*>& container, const char* name) {
        for (auto& i : container) {
            if (!strcmp(name, i)) return;
        }
        container.push_back(name);
    }

    VkResult CreateDebugMessenger() {
        // 设置调试回调函数
        static PFN_vkDebugUtilsMessengerCallbackEXT DebugUtilsMessengerCallback =
            [](VkDebugUtilsMessageSeverityFlagBitsEXT meeeageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) -> VkBool32 {
            std::cout << std::format("{}\n\n", pCallbackData->pMessage);
            return VK_FALSE;
        };
        // 创建调试信使结构体，只关心警告和错误信息
        VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = DebugUtilsMessengerCallback};
        // 获取函数指针
        PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            // Vulkan中扩展相关的函数大多通过vkGetInstanceProcAddr获取
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        // 创建调试信使
        if (vkCreateDebugUtilsMessenger) {
            VkResult result = vkCreateDebugUtilsMessenger(instance, &debugUtilsMessengerCreateInfo, nullptr, &debugMessenger);
            if (result) {
                std::cout << std::format("[ graphicsBase ] ERROR\nFailed to create a debug messenger!\nError code: {}\n",
                                         static_cast<int32_t>(result));
            }
            return result;
        }
        std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the function pointer of vkCreateDebugUtilsMessengerEXT!\n");
        return VK_RESULT_MAX_ENUM;
    }

    // 检查并取得所需的队列簇索引
    VkResult GetQueueFamilyIndices(VkPhysicalDevice physicalDevice) {
        // 获取物理设备的队列簇属性
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        if (!queueFamilyCount) return VK_RESULT_MAX_ENUM;
        std::vector<VkQueueFamilyProperties> queueFamilyProertieses(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProertieses.data());
        // 查找所需的队列簇
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            VkBool32 supportGraphics = queueFamilyProertieses[i].queueFlags & VK_QUEUE_GRAPHICS_BIT, suppoortPresentation = false,
                     supportCompute = queueFamilyProertieses[i].queueFlags & VK_QUEUE_COMPUTE_BIT;
            // 只在创建了window surface 时获取支持呈现的队列簇索引
            if (surface) {
                if (VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &suppoortPresentation)) {
                    std::cout << std::format(
                        "[ graphicsBase ] ERROR\nFailed to determine if the queue family supports presentation!\nError code: {}\n",
                        static_cast<int32_t>(result));
                    return result;
                }
            }
            if (supportGraphics && supportCompute && (!surface || suppoortPresentation)) {
                queueFamilyIndex_graphics = queueFamilyIndex_compute = i;
                if (surface) queueFamilyIndex_presentation = i;
                return VK_SUCCESS;
            }
        }
        return VK_RESULT_MAX_ENUM;
    }

    VkResult CreateSwapchain_Internal() {}

   public:
    // Getter
    uint32_t ApiVersion() const { return apiVersion; }
    VkInstance Instance() const { return instance; }
    const std::vector<const char*>& InstanceLayers() const { return instanceLayers; }
    const std::vector<const char*>& InstanceExtensions() const { return instanceExtensions; }
    VkSurfaceKHR Surface() const { return surface; }
    VkPhysicalDevice PhysicalDevice() const { return physicalDevice; }
    const VkPhysicalDeviceProperties& PhysicalDeviceProperties() const { return physicalDeviceProperties; }
    const VkPhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties() const { return physicalDeviceMemoryProperties; }
    VkPhysicalDevice AvailablePhysicalDevice(uint32_t index) const { return availablePhysicalDevices[index]; }
    VkDevice Device() const { return device; }
    const std::vector<const char*>& DeviceExtensions() const { return deviceExtensions; }
    uint32_t QueueFamilyIndex_Graphics() const { return queueFamilyIndex_graphics; }
    uint32_t QueueFamilyIndex_Presentation() const { return queueFamilyIndex_presentation; }
    uint32_t QueueFamilyIndex_Compute() const { return queueFamilyIndex_compute; }
    VkQueue Queue_Graphics() const { return queue_graphics; }
    VkQueue Queue_Presentation() const { return queue_presentation; }
    VkQueue Queue_Compute() const { return queue_compute; }
    const VkFormat& AvailableSurfaceFormat(const uint32_t index) const { return availableSurfaceFormats[index].format; }
    const VkColorSpaceKHR& AvailableSurfaceColorSpace(const uint32_t index) const { return availableSurfaceFormats[index].colorSpace; }
    uint32_t AvailableSurfaceFormatCount() const { return static_cast<uint32_t>(availableSurfaceFormats.size()); }
    VkSwapchainKHR Swapchain() const { return swapchain; }
    VkImage SwapchainImage(const uint32_t index) const { return swapchainImages[index]; }
    VkImageView SwapchainImageView(const uint32_t index) const { return swapchainImageViews[index]; }
    uint32_t SwapchainImageCount() const { return static_cast<uint32_t>(swapchainImages.size()); }
    const VkSwapchainCreateInfoKHR& SwapchainCreateInfo() const { return swapchainCreateInfo; }

    // 以下函数用于创建Vulkan实例前
    void AddInstanceLayer(const char* layerName) { AddLayerOrExtension(instanceLayers, layerName); }

    void AddInstanceExtension(const char* extensionName) { AddLayerOrExtension(instanceExtensions, extensionName); }

    // 创建Vulkan实例
    VkResult CreateInstance(VkInstanceCreateFlags flags = 0) {
        // 在调试模式下，默认启用验证层和调试扩展
#ifndef NDEBUG
        AddInstanceLayer("VK_LAYER_KHRONOS_validation");
        AddInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
        VkApplicationInfo appInfo = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .apiVersion = apiVersion,
        };
        VkInstanceCreateInfo instanceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .flags = flags,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
            .ppEnabledLayerNames = instanceLayers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
            .ppEnabledExtensionNames = instanceExtensions.data(),
        };
        if (VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance)) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to create a vulkan instance!\nError code: {}\n", static_cast<uint32_t>(result));
            return result;
        }
#ifndef NDEBUG
        CreateDebugMessenger();
#endif
        return VK_SUCCESS;
    }

    // 创建Vulkan实例失败后，检查所需的层是否可用
    VkResult CheckInstanceLayers(std::span<const char*> layersToCheck) {
        uint32_t layerCount = 0;
        std::vector<VkLayerProperties> availableLayers;
        if (const VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr)) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance layers!\n");
            return result;
        }

        if (layerCount) {
            availableLayers.resize(layerCount);
            if (const VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data())) {
                std::cout << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance layer properties!\nError code: {}\n",
                                         static_cast<uint32_t>(result));
                return result;
            }
            for (auto& i : layersToCheck) {
                bool found = false;
                for (const auto& j : availableLayers) {
                    if (strcmp(i, j.layerName) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) i = nullptr;
            }
        } else {
            for (auto& i : layersToCheck) i = nullptr;
        }
        return VK_SUCCESS;
    }

    VkResult CheckInstanceExtensions(std::span<const char*> extensionsToCheck, const char* layerName = nullptr) const {
        uint32_t extensionCount;
        std::vector<VkExtensionProperties> availableExtensions;
        if (const VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, nullptr)) {
            layerName
                ? std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\nLayer name:{}\n", layerName)
                : std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\n");
            return result;
        }
        if (extensionCount) {
            availableExtensions.resize(extensionCount);
            if (const VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, availableExtensions.data())) {
                std::cout << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance extension properties!\nError code: {}\n",
                                         static_cast<uint32_t>(result));
                return result;
            }
            for (auto& i : extensionsToCheck) {
                bool found = false;
                for (const auto& j : availableExtensions)
                    if (!strcmp(i, j.extensionName)) {
                        found = true;
                        break;
                    }
                if (!found) i = nullptr;
            }
        } else
            for (auto& i : extensionsToCheck) i = nullptr;
        return VK_SUCCESS;
    }

    void InstanceLayers(const std::vector<const char*>& layerNames) { instanceLayers = layerNames; }
    void InstanceExtensions(const std::vector<const char*>& extensionNames) { instanceExtensions = extensionNames; }

    // 该函数用于选择物理设备前
    void Surface(VkSurfaceKHR surface) {
        if (!this->surface) this->surface = surface;
    }

    // 用于创建逻辑设备前
    void AddDeviceExtension(const char* extensionName) { AddLayerOrExtension(deviceExtensions, extensionName); }

    // 获取物理设备
    VkResult GetPhysicalDevices() {
        uint32_t deviceCount;
        if (VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr)) {
            return result;
        }
        if (!deviceCount) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to find any physical device supports vulkan!\n"), abort();
        }
        availablePhysicalDevices.resize(deviceCount);
        VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, availablePhysicalDevices.data());
        if (result) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to enumerate physical devices!\nError code: {}\n", static_cast<int32_t>(result));
        }
        return result;
    }

    // 选择物理设备，并获得所需的队列族索引
    VkResult DeterminePhysicalDevice(uint32_t deviceIndex = 0, bool enableGraphicsQueue = true, bool enableComputeQueue = true) {
        // 定义一个特殊值用于标记一个队列簇索引已被找过但是为找到
        static constexpr uint32_t notFound = INT32_MAX;
        static std::vector<uint32_t> queueFamilyIndices(availablePhysicalDevices.size());

        if (queueFamilyIndices[deviceIndex] == notFound) {
            return VK_RESULT_MAX_ENUM;
        }

        // 如果队列簇索引被获取但还未被找到
        if (queueFamilyIndices[deviceIndex] == VK_QUEUE_FAMILY_IGNORED) {
            VkResult result = GetQueueFamilyIndices(availablePhysicalDevices[deviceIndex]);
            if (result) {
                if (result == VK_RESULT_MAX_ENUM) {
                    queueFamilyIndices[deviceIndex] = notFound;
                }
                return result;
            } else {
                queueFamilyIndices[deviceIndex] = queueFamilyIndex_graphics;
            }
        } else {
            queueFamilyIndex_graphics = queueFamilyIndex_compute = queueFamilyIndices[deviceIndex];
            queueFamilyIndex_presentation = surface ? queueFamilyIndices[deviceIndex] : VK_QUEUE_FAMILY_IGNORED;
        }

        physicalDevice = availablePhysicalDevices[deviceIndex];
        return VK_SUCCESS;
    }

    // 创建逻辑设备，并获得队列
    VkResult CreateDevice(VkDeviceCreateFlags flags = 0) {
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfos[3] = {
            {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .pQueuePriorities = &queuePriority},
            {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .pQueuePriorities = &queuePriority},
            {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .pQueuePriorities = &queuePriority}};
        uint32_t queueCreateInfoCount = 0;
        if (queueFamilyIndex_graphics != VK_QUEUE_FAMILY_IGNORED) {
            queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_graphics;
        }
        if (queueFamilyIndex_presentation != VK_QUEUE_FAMILY_IGNORED && queueFamilyIndex_presentation != queueFamilyIndex_graphics) {
            queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_presentation;
        }
        if (queueFamilyIndex_compute != VK_QUEUE_FAMILY_IGNORED && queueFamilyIndex_compute != queueFamilyIndex_graphics &&
            queueFamilyIndex_compute != queueFamilyIndex_presentation) {
            queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_compute;
        }

        // 获取物理设备特性
        VkPhysicalDeviceFeatures physicalDeviceFeatures;
        vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);

        VkDeviceCreateInfo deviceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .flags = flags,
            .queueCreateInfoCount = queueCreateInfoCount,
            .pQueueCreateInfos = queueCreateInfos,
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
            .pEnabledFeatures = &physicalDeviceFeatures,
        };

        if (VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device)) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to create a vulkan logical device!\nError code: {}\n",
                                     static_cast<int32_t>(result));
            return result;
        }

        if (queueFamilyIndex_graphics != VK_QUEUE_FAMILY_IGNORED) {
            vkGetDeviceQueue(device, queueFamilyIndex_graphics, 0, &queue_graphics);
        }
        if (queueFamilyIndex_presentation != VK_QUEUE_FAMILY_IGNORED) {
            vkGetDeviceQueue(device, queueFamilyIndex_presentation, 0, &queue_presentation);
        }
        if (queueFamilyIndex_compute != VK_QUEUE_FAMILY_IGNORED) {
            vkGetDeviceQueue(device, queueFamilyIndex_compute, 0, &queue_compute);
        }

        // 获取物理设备属性和内存属性
        vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);
        // 输出所选物理设备的名称
        std::cout << std::format("Renderer: {}\n", physicalDeviceProperties.deviceName);

        return VK_SUCCESS;
    }

    // 创建逻辑设备失败后，检查所需的扩展是否可用
    VkResult CheckDeviceExtensions(std::span<const char*>& extensionsToCheck, const char* layerName = nullptr) const {}

    void DeviceExtensions(const std::vector<const char*>& extensionNames) { deviceExtensions = extensionNames; }

    VkResult GetSurfaceFormats() {}

    VkResult SetSurfaceFormat(VkSurfaceFormatKHR surfaceFormat) {}

    // 创建交换链
    VkResult CreateSwapchain(bool limitFrameRate = true, VkSwapchainCreateFlagsKHR flags = 0) {}

    VkResult RecreateSwapchain() {}

    // 静态函数、用于访问单例
    static graphicsBase& Base() { return singleton; }
};

inline graphicsBase graphicsBase::singleton;
}  // namespace vulkan
