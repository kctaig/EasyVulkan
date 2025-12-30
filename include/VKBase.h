#pragma once

#include "EasyVKStart.h"
#define VK_RESULT_THROW

#define DestroyHandleBy(Func)                                 \
    if (handle) {                                             \
        Func(graphicsBase::Base().Device(), handle, nullptr); \
        handle = VK_NULL_HANDLE;                              \
    }

#define MoveHandle         \
    handle = other.handle; \
    other.handle = VK_NULL_HANDLE;

#define DefineHandleTypeOperator \
    operator decltype(handle)() const { return handle; }

#define DefineAddressFunction \
    const decltype(handle)* Address() const { return &handle; }

#ifndef NDEBUG
#define ENABLE_DEBUG_MESSENGER true
#else
#define ENABLE_DEBUG_MESSENGER false
#endif

namespace vulkan {
constexpr VkExtent2D defaultWindowSize = {1280, 720};
inline auto& outStream = std::cout;

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
        VkResult localResult = this->result;
        this->result = VK_SUCCESS;
        return localResult;
    }
};
inline void (*result_t::callback_throw)(VkResult);

// 情况2：若抛弃返回值，让编译器发出警告
#elif defined(VK_RESULT_NODISCARD)
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

    uint32_t currentImageIndex = 0;

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
            outStream << std::format("{}\n\n", pCallbackData->pMessage);
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
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a debug messenger!\nError code: {}\n",
                                         static_cast<int32_t>(result));
            }
            return result;
        }
        outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the function pointer of vkCreateDebugUtilsMessengerEXT!\n");
        return VK_RESULT_MAX_ENUM;
    }

    // 检查并取得所需的队列簇索引
    result_t GetQueueFamilyIndices(VkPhysicalDevice physicalDevice) {
        // 获取物理设备的队列簇属性
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        if (!queueFamilyCount) return VK_RESULT_MAX_ENUM;
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());
        // 查找所需的队列簇
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            VkBool32 supportGraphics = queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT, supportPresentation = false,
                     supportCompute = queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT;
            // 只在创建了window surface 时获取支持呈现的队列簇索引
            if (surface) {
                if (VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportPresentation)) {
                    outStream << std::format(
                        "[ graphicsBase ] ERROR\nFailed to determine if the queue family supports presentation!\nError code: {}\n",
                        static_cast<int32_t>(result));
                    return result;
                }
            }
            if (supportGraphics && supportCompute && (!surface || supportPresentation)) {
                queueFamilyIndex_graphics = queueFamilyIndex_compute = i;
                if (surface) queueFamilyIndex_presentation = i;
                return VK_SUCCESS;
            }
        }
        return VK_RESULT_MAX_ENUM;
    }

    result_t CreateSwapchain_Internal() {
        // 创建交换链
        if (VkResult result = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain)) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a swapchain!\nError code: {}\n", static_cast<int32_t>(result));
            return result;
        }
        // 获取交换链图像
        uint32_t swapchainImageCount;
        if (VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr)) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of swapchain images!\nError code: {}\n",
                                     static_cast<int32_t>(result));
            return result;
        }
        swapchainImages.resize(swapchainImageCount);
        if (VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data())) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to get swapchain images!\nError code: {}\n", static_cast<int32_t>(result));
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
            if (VkResult result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &swapchainImageViews[i])) {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a swapchain image view!\nError code: {}\n",
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
    // 静态函数、用于访问单例
    static graphicsBase& Base() { return singleton; }

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
    uint32_t CurrentImageIndex() const { return currentImageIndex; }

    // 添加回调函数
    void AddCallback_CreateSwapchain(void (*function)()) { callbacks_createSwapchain.push_back(function); }
    void AddCallback_DestroySwapchain(void (*function)()) { callbacks_destroySwapchain.push_back(function); }
    void AddCallback_CreateDevice(void (*function)()) { callbacks_createDevice.push_back(function); }
    void AddCallback_DestroyDevice(void (*function)()) { callbacks_destroyDevice.push_back(function); }

    result_t WaitIdle() const {
        VkResult result = vkDeviceWaitIdle(device);
        if (result) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to wait for the device to be idle!\nError code: {}\n",
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
        if constexpr (ENABLE_DEBUG_MESSENGER) {
            AddInstanceLayer("VK_LAYER_KHRONOS_validation");
            AddInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

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
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a vulkan instance!\nError code: {}\n", static_cast<uint32_t>(result));
            return result;
        }
        if constexpr (ENABLE_DEBUG_MESSENGER) {
            CreateDebugMessenger();
        }
        return VK_SUCCESS;
    }

    // 创建Vulkan实例失败后，检查所需的层是否可用
    result_t CheckInstanceLayers(std::span<const char*> layersToCheck) {
        uint32_t layerCount = 0;
        std::vector<VkLayerProperties> availableLayers;
        if (const VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr)) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance layers!\n");
            return result;
        }

        if (layerCount) {
            availableLayers.resize(layerCount);
            if (const VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data())) {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance layer properties!\nError code: {}\n",
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
        if (const VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, nullptr)) {
            layerName
                ? outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\nLayer name:{}\n", layerName)
                : outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\n");
            return result;
        }
        if (extensionCount) {
            availableExtensions.resize(extensionCount);
            if (const VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, availableExtensions.data())) {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance extension properties!\nError code: {}\n",
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
        if (VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr)) {
            return result;
        }
        if (!deviceCount) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to find any physical device supports vulkan!\n"), abort();
        }
        availablePhysicalDevices.resize(deviceCount);
        VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, availablePhysicalDevices.data());
        if (result) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to enumerate physical devices!\nError code: {}\n", static_cast<int32_t>(result));
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

        if (VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device)) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a vulkan logical device!\nError code: {}\n",
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
        outStream << std::format("Renderer: {}\n", physicalDeviceProperties.deviceName);
        ExecuteCallbacks(callbacks_createDevice);
        return VK_SUCCESS;
    }

    // 创建逻辑设备失败后，检查所需的扩展是否可用
    result_t CheckDeviceExtensions(std::span<const char*>& extensionsToCheck, const char* layerName = nullptr) const {}

    void DeviceExtensions(const std::vector<const char*>& extensionNames) { deviceExtensions = extensionNames; }

    result_t GetSurfaceFormats() {
        uint32_t surfaceFormatCount;
        if (VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr)) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of surface formats!\nError code: {}\n",
                                     static_cast<int32_t>(result));
            return result;
        }
        if (!surfaceFormatCount) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to find any supported surface format!\n"), abort();
        }
        availableSurfaceFormats.resize(surfaceFormatCount);
        VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, availableSurfaceFormats.data());
        if (result) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to get surface formats!\nError code: {}\n", static_cast<int32_t>(result));
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
        if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities)) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: {}\n",
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
                    swapchainCreateInfo.compositeAlpha =
                        static_cast<VkCompositeAlphaFlagBitsKHR>(surfaceCapabilities.supportedCompositeAlpha & (1 << i));
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
            outStream << std::format("[ graphicsBase ] WARNING\nVK_IMAGE_USAGE_TRANSFRE_DST_BIT isn't supported!\n");
        }
        // 指定图像格式
        if (availableSurfaceFormats.empty()) {
            if (VkResult result = GetSurfaceFormats()) {
                return result;
            }
        }
        if (!swapchainCreateInfo.imageFormat) {
            if (SetSurfaceFormat({VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}) &&
                SetSurfaceFormat({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})) {
                swapchainCreateInfo.imageFormat = availableSurfaceFormats[0].format;
                swapchainCreateInfo.imageColorSpace = availableSurfaceFormats[0].colorSpace;
                outStream << std::format("[ graphicsBase ] WARNING\nFailed to select a four-component UNORM surface format!\n");
            }
        }

        // 指定呈现模式
        uint32_t surfacePresentModeCount;
        if (VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, nullptr)) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of surface present modes!\nError code: {}\n",
                                     static_cast<int32_t>(result));
            return result;
        }
        if (!surfacePresentModeCount) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to find any surface present mode!\n"), abort();
        }
        std::vector<VkPresentModeKHR> surfacePresentModes(surfacePresentModeCount);
        if (VkResult result =
                vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, surfacePresentModes.data())) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to get surface present modes!\nError code: {}\n", static_cast<int32_t>(result));
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
        if (VkResult result = CreateSwapchain_Internal()) {
            return result;
        }
        ExecuteCallbacks(callbacks_createSwapchain);
        return VK_SUCCESS;
    }

    result_t RecreateSwapchain() {
        VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
        if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities)) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: {}\n",
                                     static_cast<int32_t>(result));
            return result;
        }
        if (surfaceCapabilities.currentExtent.width == 0 || surfaceCapabilities.currentExtent.height == 0) {
            return VK_SUBOPTIMAL_KHR;
        }
        swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
        swapchainCreateInfo.oldSwapchain = swapchain;

        VkResult result = vkQueueWaitIdle(queue_graphics);
        // 仅在等待图形队列成功，且图形与呈现所用队列不同时等待呈现队列
        if (!result && queue_graphics != queue_presentation) {
            result = vkQueueWaitIdle(queue_presentation);
        }
        if (result) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to wait for the queue to be idle!\nError code: {}\n",
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
        if ((result = CreateSwapchain_Internal())) {
            return result;
        }
        ExecuteCallbacks(callbacks_createSwapchain);
        return VK_SUCCESS;
    }

    // 该函数用于获取交换链图像索引到currentImageIndex
    result_t SwapImage(VkSemaphore semaphore_imageIsAvailable) {
        // 摧毁旧的交换链
        if (swapchainCreateInfo.oldSwapchain && swapchainCreateInfo.oldSwapchain != swapchain) {
            vkDestroySwapchainKHR(device, swapchainCreateInfo.oldSwapchain, nullptr);
            swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
        }
        while (VkResult result =
                   vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, semaphore_imageIsAvailable, VK_NULL_HANDLE, &currentImageIndex)) {
            switch (result) {
                case VK_SUBOPTIMAL_KHR:
                // 交换链与Surface不兼容，不能用于渲染，必须重建交换链
                case VK_ERROR_OUT_OF_DATE_KHR:
                    // 重新创建交换链后继续获取交换链索引
                    if (VkResult recreateResult = RecreateSwapchain()) {
                        return recreateResult;
                    }
                    break;
                default:
                    outStream << std::format("[ graphicsBase ] ERROR\nFailed to acquire the next image!\nError code: {}\n",
                                             static_cast<int32_t>(result));
                    return result;
            }
        }
        return VK_SUCCESS;
    }

    // 提交命令缓冲区到图形队列，需要自定义同步
    result_t SubmitCommandBuffer_Graphics(VkSubmitInfo& submitInfo, VkFence fence = VK_NULL_HANDLE) const {
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkResult result = vkQueueSubmit(queue_graphics, 1, &submitInfo, fence);
        if (result) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to submit the command buffer!\nError code: {}\n", static_cast<int32_t>(result));
        }
        return result;
    }

    // 渲染循环中提交命令缓冲区到图形队列
    result_t SubmitCommandBuffer_Graphics(VkCommandBuffer commandBuffer, VkSemaphore semaphore_imageIsAvailable = VK_NULL_HANDLE,
                                          VkSemaphore semaphore_renderingIsOver = VK_NULL_HANDLE, VkFence fence = VK_NULL_HANDLE,
                                          VkPipelineStageFlags waitDstStage_imageIsAvailable = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) const {
        VkSubmitInfo submitInfo = {.commandBufferCount = 1, .pCommandBuffers = &commandBuffer};
        // 命令缓冲区需要等待semaphore_imageIsAvailable
        if (semaphore_imageIsAvailable) {
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &semaphore_imageIsAvailable;
            // 默认参数VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT表示在颜色附件输出阶段等待
            submitInfo.pWaitDstStageMask = &waitDstStage_imageIsAvailable;
        }
        // 命令缓冲区执行完后需要发送semaphore_renderingIsOver
        if (semaphore_renderingIsOver) {
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &semaphore_renderingIsOver;
        }
        return SubmitCommandBuffer_Graphics(submitInfo, fence);
    }

    // 提交单个命令缓冲区到图形队列,只使用栅栏
    result_t SubmitCommandBuffer_Graphics(VkCommandBuffer commandBuffer, VkFence fence = VK_NULL_HANDLE) const {
        VkSubmitInfo submitInfo = {.commandBufferCount = 1, .pCommandBuffers = &commandBuffer};
        return SubmitCommandBuffer_Graphics(submitInfo, fence);
    }

    // 提交命令缓冲区到计算队列，需要自定义同步
    result_t SubmitCommandBuffer_Compute(VkSubmitInfo& submitInfo, VkFence fence = VK_NULL_HANDLE) const {
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkResult result = vkQueueSubmit(queue_compute, 1, &submitInfo, fence);
        if (result) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to submit the command buffer!\nError code: {}\n", static_cast<int32_t>(result));
        }
        return result;
    }

    // 提交命令缓冲区到计算队列,只使用栅栏
    result_t SubmitCommandBuffer_Compute(VkCommandBuffer commandBuffer, VkFence fence = VK_NULL_HANDLE) const {
        VkSubmitInfo submitInfo = {.commandBufferCount = 1, .pCommandBuffers = &commandBuffer};
        return SubmitCommandBuffer_Compute(submitInfo, fence);
    }

    result_t PresentImage(VkPresentInfoKHR& presentInfo) {
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        switch (VkResult result = vkQueuePresentKHR(queue_presentation, &presentInfo)) {
            case VK_SUCCESS:
                return VK_SUCCESS;
            case VK_SUBOPTIMAL_KHR:
            case VK_ERROR_OUT_OF_DATE_KHR:
                return RecreateSwapchain();
            default:
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to queue the image for presentation!\nError code: {}\n",
                                         static_cast<int32_t>(result));
                return result;
        }
    }

    //  渲染循环中提交交换链图像到呈现队列
    result_t PresentImage(VkSemaphore semaphore_renderingIsOver = VK_NULL_HANDLE) {
        VkPresentInfoKHR presentInfo = {
            .swapchainCount = 1,                 // 需要被呈现的交换链的个数
            .pSwapchains = &swapchain,           // 需要被呈现的交换链的数组
            .pImageIndices = &currentImageIndex  // 交换链中需要被呈现的图像索引
        };
        // 需要等待semaphore_renderingIsOver
        if (semaphore_renderingIsOver) {
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &semaphore_renderingIsOver;
        }
        return PresentImage(presentInfo);
    }
};

class fence {
    VkFence handle = VK_NULL_HANDLE;

   public:
    // fence() = default;
    fence(VkFenceCreateInfo& createInfo) { Create(createInfo); }
    fence(VkFenceCreateFlags flags = 0) { Create(flags); }
    fence(fence&& other) noexcept { MoveHandle; }
    ~fence() { DestroyHandleBy(vkDestroyFence); }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    // Const function
    // CPU通过调用Wait函数等待栅栏信号，GPU完成工作后会设置栅栏为有信号
    result_t Wait() const {
        VkResult result = vkWaitForFences(graphicsBase::Base().Device(), 1, &handle, false, UINT64_MAX);
        if (result) {
            outStream << std::format("[ fence ] ERROR\nFailed to wait for the fence!\nError code: {}\n", static_cast<int32_t>(result));
        }
        return result;
    }
    // CPU重置栅栏以便重用
    result_t Reset() const {
        VkResult result = vkResetFences(graphicsBase::Base().Device(), 1, &handle);
        if (result) {
            outStream << std::format("[ fence ] ERROR\nFailed to reset the fence!\nError code: {}\n", static_cast<int32_t>(result));
        }
        return result;
    }
    result_t WaitAndReset() const {
        VkResult result = Wait();
        // result 为 VK_SUCCESS 时才重置栅栏
        result || (result = Reset());
        return result;
    }
    result_t Status() const {
        VkResult result = vkGetFenceStatus(graphicsBase::Base().Device(), handle);
        if (result < 0) {
            outStream << std::format("[ fence ] ERROR\nFailed to get the status of the fence!\nError code: {}\n", static_cast<int32_t>(result));
        }
        return result;
    }
    // Non-const function
    result_t Create(VkFenceCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkResult result = vkCreateFence(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result) {
            outStream << std::format("[ fence ] ERROR\nFailed to create a fence!\nError code: {}\n", static_cast<int32_t>(result));
        }
        return result;
    }
    result_t Create(VkFenceCreateFlags flags = 0) {
        VkFenceCreateInfo createInfo = {.flags = flags};
        return Create(createInfo);
    }
};

class semaphore {
    VkSemaphore handle = VK_NULL_HANDLE;

   public:
    semaphore(VkSemaphoreCreateInfo& createInfo) { Create(createInfo); }
    semaphore(/*VkSemaphoreCreateFlags flags*/) { Create(); }
    semaphore(semaphore&& other) noexcept { MoveHandle; }
    ~semaphore() { DestroyHandleBy(vkDestroySemaphore); }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    // Non-const function
    result_t Create(VkSemaphoreCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkResult result = vkCreateSemaphore(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result) {
            outStream << std::format("[ semaphore ] ERROR\nFailed to create a semaphore!\nError code: {}\n", static_cast<int32_t>(result));
        }
        return result;
    }
    result_t Create(/*VkSemaphoreCreateFlags flags = 0*/) {
        VkSemaphoreCreateInfo createInfo = {};
        return Create(createInfo);
    }
};

class commandBuffer {
    // commandPool负责分配和释放commandBuffer, 需让其能访问私有成员handle
    friend class commandPool;
    VkCommandBuffer handle = VK_NULL_HANDLE;

   public:
    commandBuffer() = default;
    commandBuffer(commandBuffer&& other) noexcept { MoveHandle; }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    // Const function
    result_t Begin(VkCommandBufferUsageFlags usageFlags, VkCommandBufferInheritanceInfo& inheritanceInfo) const {
        inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = usageFlags,
            .pInheritanceInfo = &inheritanceInfo,
        };
        VkResult result = vkBeginCommandBuffer(handle, &beginInfo);
        if (result) {
            outStream << std::format("[ commandBuffer ] ERROR\nFailed to begin a command buffer!\nError code: {}\n", static_cast<int32_t>(result));
        }
        return result;
    }
    result_t Begin(VkCommandBufferUsageFlags usageFlags = 0) const {
        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = usageFlags,
        };
        VkResult result = vkBeginCommandBuffer(handle, &beginInfo);
        if (result) {
            outStream << std::format("[ commandBuffer ] ERROR\nFailed to begin a command buffer!\nError code: {}\n", static_cast<int32_t>(result));
        }
        return result;
    }
    result_t End() const {
        VkResult result = vkEndCommandBuffer(handle);
        if (result) {
            outStream << std::format("[ commandBuffer ] ERROR\nFailed to end a command buffer!\nError code: {}\n", static_cast<int32_t>(result));
        }
        return result;
    }
};

class commandPool {
    VkCommandPool handle = VK_NULL_HANDLE;

   public:
    commandPool() = default;
    commandPool(VkCommandPoolCreateInfo& createInfo) { Create(createInfo); }
    commandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0) { Create(queueFamilyIndex, flags); }
    commandPool(commandPool&& other) noexcept { MoveHandle; }
    ~commandPool() { DestroyHandleBy(vkDestroyCommandPool); }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    // Const function
    result_t AllocateBuffers(arrayRef<VkCommandBuffer> buffers, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const {
        VkCommandBufferAllocateInfo allocateInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                    .commandPool = handle,
                                                    .level = level,
                                                    .commandBufferCount = static_cast<uint32_t>(buffers.Count())};
        VkResult result = vkAllocateCommandBuffers(graphicsBase::Base().Device(), &allocateInfo, buffers.Pointer());
        if (result) {
            outStream << std::format("[ commandPool ] ERROR\nFailed to allocate command buffers!\nError code: {}\n", static_cast<int32_t>(result));
        }
        return result;
    }
    result_t AllocateBuffers(arrayRef<commandBuffer> buffers, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const {
        return AllocateBuffers({&buffers[0].handle, buffers.Count()}, level);
    }
    void FreeBuffers(arrayRef<VkCommandBuffer> buffers) const {
        vkFreeCommandBuffers(graphicsBase::Base().Device(), handle, buffers.Count(), buffers.Pointer());
        memset(buffers.Pointer(), 0, buffers.Count() * sizeof(VkCommandBuffer));
    }
    void FreeBuffers(arrayRef<commandBuffer> buffers) const { FreeBuffers({&buffers[0].handle, buffers.Count()}); }
    // Non-const function
    result_t Create(VkCommandPoolCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        VkResult result = vkCreateCommandPool(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result) {
            outStream << std::format("[ commandPool ] ERROR\nFailed to create a command pool!\nError code: {}\n", static_cast<int32_t>(result));
        }
        return result;
    }
    result_t Create(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0) {
        VkCommandPoolCreateInfo createInfo = {.flags = flags, .queueFamilyIndex = queueFamilyIndex};
        return Create(createInfo);
    }
};

class renderPass {
    VkRenderPass handle = VK_NULL_HANDLE;

   public:
    renderPass() = default;
    renderPass(VkRenderPassCreateInfo& createInfo) { Create(createInfo); }
    renderPass(renderPass&& other) noexcept { MoveHandle; }
    ~renderPass() { DestroyHandleBy(vkDestroyRenderPass); }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    // Const function
    void CmdBegin(VkCommandBuffer commandBuffer, VkRenderPassBeginInfo& beginInfo,
                  VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const {
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.renderPass = handle;
        vkCmdBeginRenderPass(commandBuffer, &beginInfo, subpassContents);
    }
    void CmdBegin(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, VkRect2D renderArea, arrayRef<const VkClearValue> clearValues = {},
                  VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const {
        VkRenderPassBeginInfo beginInfo = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                           .renderPass = handle,
                                           .framebuffer = framebuffer,
                                           .renderArea = renderArea,
                                           .clearValueCount = static_cast<uint32_t>(clearValues.Count()),
                                           .pClearValues = clearValues.Pointer()};
        vkCmdBeginRenderPass(commandBuffer, &beginInfo, subpassContents);
    }
    void CmdNext(VkCommandBuffer commandBuffer, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const {
        vkCmdNextSubpass(commandBuffer, subpassContents);
    }
    void CmdEnd(VkCommandBuffer commandBuffer) const { vkCmdEndRenderPass(commandBuffer); }
    // Non-const function
    result_t Create(VkRenderPassCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        VkResult result = vkCreateRenderPass(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result) {
            outStream << std::format("[ renderPass ] ERROR\nFailed to create a render pass!\nError code: {}\n", static_cast<int32_t>(result));
        }
        return result;
    }
};

class framebuffer {
    VkFramebuffer handle = VK_NULL_HANDLE;

   public:
    framebuffer() = default;
    framebuffer(VkFramebufferCreateInfo& createInfo) { Create(createInfo); }
    framebuffer(framebuffer&& other) noexcept { MoveHandle; }
    ~framebuffer() { DestroyHandleBy(vkDestroyFramebuffer); }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    // Non-const function
    result_t Create(VkFramebufferCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        VkResult result = vkCreateFramebuffer(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result) {
            outStream << std::format("[ framebuffer ] ERROR\nFailed to create a framebuffer!\nError code: {}\n", static_cast<int32_t>(result));
        }
        return result;
    }
};

inline graphicsBase graphicsBase::singleton;
}  // namespace vulkan
