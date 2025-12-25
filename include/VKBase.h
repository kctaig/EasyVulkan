#pragma once

#include "EasyVKStart.h"

namespace vulkan {
constexpr VkExtent2D defaultWindowSize = {1280, 720};

// 情况1：根据函数返回值确定是否抛出异常
#ifdef VK_RESULT_THROW
class result_t {
    VkResult result;

   public:
    // 静态函数指针成员变量
    static void (*callback_throw)(VkResult);
    result_t(VkResult result) : result(result) {}
    // 防止重复处理错误：错误从一个对象传递到另一个对象时，原对象的错误状态被清除
    result_t(result_t&& other) noexcept : result(other.result) { other.result = VK_SUCCESS; }
    // 防止错误没有被处理
    ~result_t() noexcept(false) {
        if (static_cast<uint32_t>(result) < VK_RESULT_MAX_ENUM) return;
        if (callback_throw) {
            callback_throw(result);
        }
        throw result;
    };
    // operator VkResult() : 表示转换为VkResult类型的运算符
    operator VkResult() {
        VkResult result = this->result;
        this->result = VK_SUCCESS;
        return result;
    }
};
inline void (*result_t::callback_throw)(VkResult);

// 情况2：若抛弃返回值，让编译器发出警告
#elifdef VK_RESULT_NODISCARD
struct [[nodiscard]] result_t {
    VkResult result;
    result_t(VkResult result) : result(result) {}
    operator VkResult() const { return result; }
};

#pragma warning(disable : 4834)  // 禁用“丢弃返回值”的警告
#pragma warning(disable : 6031)  // 禁用“忽略返回值”的警告

// 情况3：啥也不干
#else
using result_t = VkResult;
#endif

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

    // 回调函数列表
    std::vector<void (*)()> callbacks_createSwapchain;
    std::vector<void (*)()> callbacks_destroySwapchain;
    std::vector<void (*)()> callbacks_createDevice;
    std::vector<void (*)()> callbacks_destroyDevice;

    /******* private function *********/

    graphicsBase() = default;
    graphicsBase(graphicsBase&&) = delete;
    ~graphicsBase() {
        if (!instance) {
            return;
        }
        if (device) {
            WaitIdle();
            if (swapchain) {
                ExecuteCallbacks(callbacks_destroySwapchain);
                for (auto& i : swapchainImageViews) {
                    if (i) {
                        vkDestroyImageView(device, i, nullptr);
                    }
                }
                vkDestroySwapchainKHR(device, swapchain, nullptr);
            }
            ExecuteCallbacks(callbacks_destroyDevice);
            vkDestroyDevice(device, nullptr);
        }
        if (surface) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
        }
        if (debugMessenger) {
            PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUitlsMessenger =
                reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (vkDestroyDebugUitlsMessenger) {
                vkDestroyDebugUitlsMessenger(instance, debugMessenger, nullptr);
            }
        }
        vkDestroyInstance(instance, nullptr);
    }

    // 添加实例层或扩展
    static void AddLayerOrExtension(std::vector<const char*>& container, const char* name) {
        for (auto& i : container) {
            if (!strcmp(name, i)) return;
        }
        container.push_back(name);
    }

    result_t CreateDebugMessenger() {
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
            result_t result = vkCreateDebugUtilsMessenger(instance, &debugUtilsMessengerCreateInfo, nullptr, &debugMessenger);
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
    result_t GetQueueFamilyIndices(VkPhysicalDevice physicalDevice) {
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
                if (result_t result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &suppoortPresentation)) {
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

    result_t CreateSwapchain_Internal() {
        // 创建交换链
        if (result_t result = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain)) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to create a swapchain!\nError code: {}\n", static_cast<int32_t>(result));
            return result;
        }
        // 获取交换链图像
        uint32_t swapchainImageCount;
        if (result_t result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr)) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of swapchain images!\nError code: {}\n",
                                     static_cast<int32_t>(result));
            return result;
        }
        swapchainImages.resize(swapchainImageCount);
        if (result_t result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data())) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get swapchain images!\nError code: {}\n", static_cast<int32_t>(result));
            return result;
        }
        // 创建交换链图像视图
        swapchainImageViews.resize(swapchainImageCount);
        VkImageViewCreateInfo imageViewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchainCreateInfo.imageFormat,
            .components = {},
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };
        for (size_t i = 0; i < swapchainImageCount; i++) {
            imageViewCreateInfo.image = swapchainImages[i];
            if (result_t result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &swapchainImageViews[i])) {
                std::cout << std::format("[ graphicsBase ] ERROR\nFailed to create a swapchain image view!\nError code: {}\n",
                                         static_cast<int32_t>(result));
                return result;
            }
        }
        return VK_SUCCESS;
    }

    static void ExecuteCallbacks(std::vector<void (*)()> callbacks) {
        for (size_t size = callbacks.size(), i = 0; i < size; i++) {
            callbacks[i]();
        }
    }

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

    // 添加回调函数
    void AddCallback_CreateSwapchin(void (*function)()) { callbacks_createSwapchain.push_back(function); }
    void AddCallback_DestroySwapchain(void (*function)()) { callbacks_destroySwapchain.push_back(function); }
    void AddCallback_CreateDevice(void (*function)()) { callbacks_createDevice.push_back(function); }
    void AddCallback_DestroyDevice(void (*function)()) { callbacks_destroyDevice.push_back(function); }

    result_t WaitIdle() const {
        result_t result = vkDeviceWaitIdle(device);
        if (result) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to wait for the device to be idle!\nError code: {}\n",
                                     static_cast<int32_t>(result));
        }
        return result;
    }

    void Terminate() {
        this->~graphicsBase();
        instance = VK_NULL_HANDLE;
        physicalDevice = VK_NULL_HANDLE;
        device = VK_NULL_HANDLE;
        surface = VK_NULL_HANDLE;
        swapchain = VK_NULL_HANDLE;
        swapchainImages.resize(0);
        swapchainImageViews.resize(0);
        swapchainCreateInfo = {};
        debugMessenger = VK_NULL_HANDLE;
    }

    // 以下函数用于创建Vulkan实例前
    void AddInstanceLayer(const char* layerName) { AddLayerOrExtension(instanceLayers, layerName); }
    void AddInstanceExtension(const char* extensionName) { AddLayerOrExtension(instanceExtensions, extensionName); }

    // 创建Vulkan实例
    result_t CreateInstance(VkInstanceCreateFlags flags = 0) {
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
        if (result_t result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance)) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to create a vulkan instance!\nError code: {}\n", static_cast<uint32_t>(result));
            return result;
        }
#ifndef NDEBUG
        CreateDebugMessenger();
#endif
        return VK_SUCCESS;
    }

    // 创建Vulkan实例失败后，检查所需的层是否可用
    result_t CheckInstanceLayers(std::span<const char*> layersToCheck) {
        uint32_t layerCount = 0;
        std::vector<VkLayerProperties> availableLayers;
        if (const result_t result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr)) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance layers!\n");
            return result;
        }

        if (layerCount) {
            availableLayers.resize(layerCount);
            if (const result_t result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data())) {
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

    result_t CheckInstanceExtensions(std::span<const char*> extensionsToCheck, const char* layerName = nullptr) const {
        uint32_t extensionCount;
        std::vector<VkExtensionProperties> availableExtensions;
        if (const result_t result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, nullptr)) {
            layerName
                ? std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\nLayer name:{}\n", layerName)
                : std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\n");
            return result;
        }
        if (extensionCount) {
            availableExtensions.resize(extensionCount);
            if (const result_t result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, availableExtensions.data())) {
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
    result_t GetPhysicalDevices() {
        uint32_t deviceCount;
        if (result_t result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr)) {
            return result;
        }
        if (!deviceCount) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to find any physical device supports vulkan!\n"), abort();
        }
        availablePhysicalDevices.resize(deviceCount);
        result_t result = vkEnumeratePhysicalDevices(instance, &deviceCount, availablePhysicalDevices.data());
        if (result) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to enumerate physical devices!\nError code: {}\n", static_cast<int32_t>(result));
        }
        return result;
    }

    // 选择物理设备，并获得所需的队列族索引
    result_t DeterminePhysicalDevice(uint32_t deviceIndex = 0, bool enableGraphicsQueue = true, bool enableComputeQueue = true) {
        // 定义一个特殊值用于标记一个队列簇索引已被找过但是为找到
        static constexpr uint32_t notFound = INT32_MAX;
        static std::vector<uint32_t> queueFamilyIndices(availablePhysicalDevices.size());

        if (queueFamilyIndices[deviceIndex] == notFound) {
            return VK_RESULT_MAX_ENUM;
        }

        // 如果队列簇索引被获取但还未被找到
        if (queueFamilyIndices[deviceIndex] == VK_QUEUE_FAMILY_IGNORED) {
            result_t result = GetQueueFamilyIndices(availablePhysicalDevices[deviceIndex]);
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
    result_t CreateDevice(VkDeviceCreateFlags flags = 0) {
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

        if (result_t result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device)) {
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
        ExecuteCallbacks(callbacks_createDevice);
        return VK_SUCCESS;
    }

    // 创建逻辑设备失败后，检查所需的扩展是否可用
    result_t CheckDeviceExtensions(std::span<const char*>& extensionsToCheck, const char* layerName = nullptr) const {}

    void DeviceExtensions(const std::vector<const char*>& extensionNames) { deviceExtensions = extensionNames; }

    result_t GetSurfaceFormats() {
        uint32_t surfaceFormatCount;
        if (result_t result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr)) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of surface formats!\nError code: {}\n",
                                     static_cast<int32_t>(result));
            return result;
        }
        if (!surfaceFormatCount) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to find any supported surface format!\n"), abort();
        }
        availableSurfaceFormats.resize(surfaceFormatCount);
        result_t result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, availableSurfaceFormats.data());
        if (result) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get surface formats!\nError code: {}\n", static_cast<int32_t>(result));
        }
        return result;
    }

    result_t SetSurfaceFormat(VkSurfaceFormatKHR surfaceFormat) {
        bool formatIsAvailable = false;
        if (!surfaceFormat.format) {
            // 如果格式未指定，只匹配色彩空间，图像格式则选择第一个可用格式
            for (auto& i : availableSurfaceFormats) {
                if (i.colorSpace == surfaceFormat.colorSpace) {
                    swapchainCreateInfo.imageFormat = i.format;
                    swapchainCreateInfo.imageColorSpace = i.colorSpace;
                    formatIsAvailable = true;
                    break;
                }
            }
        } else {
            // 否则匹配格式和色彩空间
            for (auto& i : availableSurfaceFormats) {
                if (i.format == surfaceFormat.format && i.colorSpace == surfaceFormat.colorSpace) {
                    swapchainCreateInfo.imageFormat = i.format;
                    swapchainCreateInfo.imageColorSpace = i.colorSpace;
                    formatIsAvailable = true;
                    break;
                }
            }
        }
        if (!formatIsAvailable) {
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        }
        // 如果交换链已创建，则重新创建交换链
        if (swapchain) {
            return RecreateSwapchain();
        }
        return VK_SUCCESS;
    }

    // 创建交换链
    result_t CreateSwapchain(bool limitFrameRate = true, VkSwapchainCreateFlagsKHR flags = 0) {
        VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
        if (result_t result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities)) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: {}\n",
                                     static_cast<int32_t>(result));
            return result;
        }
        // 指定图像的数量
        swapchainCreateInfo.minImageCount =
            surfaceCapabilities.minImageCount + (surfaceCapabilities.maxImageCount > surfaceCapabilities.minImageCount);
        // 指定图像的尺寸
        swapchainCreateInfo.imageExtent =
            surfaceCapabilities.currentExtent.width == -1
                ? VkExtent2D{glm::clamp(defaultWindowSize.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width),
                             glm::clamp(defaultWindowSize.height, surfaceCapabilities.minImageExtent.height,
                                        surfaceCapabilities.maxImageExtent.height)}
                : surfaceCapabilities.currentExtent;
        // 图像的变换方式
        swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
        // 处理交换链图像透明通道的方式
        if (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
            swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        } else {
            for (size_t i = 0; i < 4; i++) {
                if (surfaceCapabilities.supportedCompositeAlpha & 1 << i) {
                    swapchainCreateInfo.compositeAlpha = VkCompositeAlphaFlagBitsKHR(surfaceCapabilities.supportedCompositeAlpha & (1 << i));
                    break;
                }
            }
        }
        // 图像的用途，图像必须被用作颜色附件
        swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
            swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }
        if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
            swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        } else {
            std::cout << std::format("[ graphicsBase ] WARNING\nVK_IMAGE_USAGE_TRANSFRE_DST_BIT isn't supported!\n");
        }
        // 指定图像格式
        if (availableSurfaceFormats.empty()) {
            if (result_t result = GetSurfaceFormats()) {
                return result;
            }
        }
        if (!swapchainCreateInfo.imageFormat) {
            if (SetSurfaceFormat({VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}) &&
                SetSurfaceFormat({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})) {
                swapchainCreateInfo.imageFormat = availableSurfaceFormats[0].format;
                swapchainCreateInfo.imageColorSpace = availableSurfaceFormats[0].colorSpace;
                std::cout << std::format("[ graphicsBase ] WARNING\nFailed to select a four-component UNORM surface format!\n");
            }
        }

        // 指定呈现模式
        uint32_t surfacePresentModeCount;
        if (result_t result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, nullptr)) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of surface present modes!\nError code: {}\n",
                                     static_cast<int32_t>(result));
            return result;
        }
        if (!surfacePresentModeCount) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to find any surface present mode!\n"), abort();
        }
        std::vector<VkPresentModeKHR> surfacePresentModes(surfacePresentModeCount);
        if (result_t result =
                vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, surfacePresentModes.data())) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get surface present modes!\nError code: {}\n", static_cast<int32_t>(result));
        }
        swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        if (!limitFrameRate) {
            for (size_t i = 0; i < surfacePresentModeCount; i++) {
                if (surfacePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                    break;
                }
            }
        }
        // 剩余参数
        swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCreateInfo.flags = flags;
        swapchainCreateInfo.surface = surface;
        swapchainCreateInfo.imageArrayLayers = 1;
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreateInfo.clipped = VK_TRUE;

        // 创建交换链
        if (result_t result = CreateSwapchain_Internal()) {
            return result;
        }
        ExecuteCallbacks(callbacks_createSwapchain);
        return VK_SUCCESS;
    }

    result_t RecreateSwapchain() {
        VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
        if (result_t result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities)) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: {}\n",
                                     static_cast<int32_t>(result));
            return result;
        }
        if (surfaceCapabilities.currentExtent.width == 0 || surfaceCapabilities.currentExtent.height == 0) {
            return VK_SUBOPTIMAL_KHR;
        }
        swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
        swapchainCreateInfo.oldSwapchain = swapchain;

        result_t result = vkQueueWaitIdle(queue_graphics);
        // 仅在等待图形队列成功，且图形与呈现所用队列不同时等待呈现队列
        if (!result && queue_graphics != queue_presentation) {
            result = vkQueueWaitIdle(queue_presentation);
        }
        if (result) {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to wait for the queue to be idle!\nError code: {}\n",
                                     static_cast<int32_t>(result));
            return result;
        }
        ExecuteCallbacks(callbacks_destroySwapchain);
        // 销毁旧的交换链图像视图
        for (auto& i : swapchainImageViews) {
            if (i) {
                vkDestroyImageView(device, i, nullptr);
            }
        }
        swapchainImageViews.resize(0);
        // 创建新的交换链
        if (result = CreateSwapchain_Internal()) {
            return result;
        }
        ExecuteCallbacks(callbacks_createSwapchain);
        return VK_SUCCESS;
    }

    // 静态函数、用于访问单例
    static graphicsBase& Base() { return singleton; }
};

inline graphicsBase graphicsBase::singleton;
}  // namespace vulkan
