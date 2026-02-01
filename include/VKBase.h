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
    static inline void (*callback_throw)(VkResult);
    result_t(VkResult result) : result(result) {}
    // 防止重复处理错误：错误从一个对象传递到另一个对象时，原对象的错误状态被清除
    result_t(result_t&& other) noexcept : result(other.result) { other.result = VK_SUCCESS; }
    // 防止错误没有被处理
    ~result_t() noexcept(false) {
        if (static_cast<uint32_t>(result) < VK_RESULT_MAX_ENUM) return;
        if (callback_throw) { callback_throw(result); }
        throw result;
    };
    // operator VkResult() : 表示转换为VkResult类型的运算符
    operator VkResult() {
        VkResult localResult = this->result;
        this->result = VK_SUCCESS;
        return localResult;
    }
};

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

class graphicsBasePlus;

class graphicsBase {
    graphicsBasePlus* pPlus = nullptr;

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
    ~graphicsBase() {
        if (!instance) { return; }
        if (device) {
            WaitIdle();
            if (swapchain) {
                ExecuteCallbacks(callbacks_destroySwapchain);
                for (auto& i : swapchainImageViews) {
                    if (i) { vkDestroyImageView(device, i, nullptr); }
                }
                vkDestroySwapchainKHR(device, swapchain, nullptr);
            }
            ExecuteCallbacks(callbacks_destroyDevice);
            vkDestroyDevice(device, nullptr);
        }
        if (surface) { vkDestroySurfaceKHR(instance, surface, nullptr); }
        if (debugMessenger) {
            auto vkDestroyDebugUtilsMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
            );
            if (vkDestroyDebugUtilsMessenger) { vkDestroyDebugUtilsMessenger(instance, debugMessenger, nullptr); }
        }
        vkDestroyInstance(instance, nullptr);
    }

    // 添加实例层或扩展
    static void AddLayerOrExtension(std::vector<const char*>& container, const char* name) {
        for (const auto& i : container) {
            if (!strcmp(name, i)) return;
        }
        container.push_back(name);
    }

    result_t CreateDebugMessenger() {
        // 设置调试回调函数
        static PFN_vkDebugUtilsMessengerCallbackEXT DebugUtilsMessengerCallback =
            [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) -> VkBool32 {
            outStream << std::format("{}\n\n", pCallbackData->pMessage);
            return VK_FALSE;
        };
        // 创建调试信使结构体，只关心警告和错误信息
        VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = DebugUtilsMessengerCallback
        };
        // 获取函数指针
        auto vkCreateDebugUtilsMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            // Vulkan中扩展相关的函数大多通过vkGetInstanceProcAddr获取
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
        );
        // 创建调试信使
        if (vkCreateDebugUtilsMessenger) {
            VkResult result =
                vkCreateDebugUtilsMessenger(instance, &debugUtilsMessengerCreateInfo, nullptr, &debugMessenger);
            if (result) {
                outStream << std::format(
                    "[ graphicsBase ] ERROR\nFailed to create a debug messenger!\nError code: {}\n",
                    static_cast<int32_t>(result)
                );
            }
            return result;
        }
        outStream << std::format(
            "[ graphicsBase ] ERROR\nFailed to get the function pointer of "
            "vkCreateDebugUtilsMessengerEXT!\n"
        );
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
            VkBool32 supportGraphics = queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT,
                     supportPresentation = false,
                     supportCompute = queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT;
            // 只在创建了window surface 时获取支持呈现的队列簇索引
            if (surface) {
                if (VkResult result =
                        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportPresentation)) {
                    outStream << std::format(
                        "[ graphicsBase ] ERROR\nFailed to determine if the queue family supports "
                        "presentation!\nError code: {}\n",
                        static_cast<int32_t>(result)
                    );
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
            outStream << std::format(
                "[ graphicsBase ] ERROR\nFailed to create a swapchain!\nError code: {}\n", static_cast<int32_t>(result)
            );
            return result;
        }
        // 获取交换链图像
        uint32_t swapchainImageCount;
        if (VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr)) {
            outStream << std::format(
                "[ graphicsBase ] ERROR\nFailed to get the count of swapchain images!\nError code: "
                "{}\n",
                static_cast<int32_t>(result)
            );
            return result;
        }
        swapchainImages.resize(swapchainImageCount);
        if (VkResult result =
                vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data())) {
            outStream << std::format(
                "[ graphicsBase ] ERROR\nFailed to get swapchain images!\nError code: {}\n",
                static_cast<int32_t>(result)
            );
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
                outStream << std::format(
                    "[ graphicsBase ] ERROR\nFailed to create a swapchain image view!\nError code: "
                    "{}\n",
                    static_cast<int32_t>(result)
                );
                return result;
            }
        }
        return VK_SUCCESS;
    }

    static void ExecuteCallbacks(std::vector<void (*)()> callbacks) {
        for (size_t size = callbacks.size(), i = 0; i < size; i++) { callbacks[i](); }
    }

  public:
    // 单例模式，禁止拷贝和移动构造
    graphicsBase(const graphicsBase&) = delete;
    graphicsBase& operator=(const graphicsBase&) = delete;
    graphicsBase(graphicsBase&&) = delete;
    graphicsBase& operator=(graphicsBase&&) = delete;
    // 第一次调用时创建单例，线程安全
    static graphicsBase& Base() {
        static graphicsBase singleton;
        return singleton;
    }
    static graphicsBasePlus& Plus() { return *Base().pPlus; }
    static void Plus(graphicsBasePlus& plus) {
        if (!Base().pPlus) { Base().pPlus = &plus; }
    }

    // Getter
    uint32_t ApiVersion() const { return apiVersion; }
    VkInstance Instance() const { return instance; }
    const std::vector<const char*>& InstanceLayers() const { return instanceLayers; }
    const std::vector<const char*>& InstanceExtensions() const { return instanceExtensions; }
    VkSurfaceKHR Surface() const { return surface; }
    VkPhysicalDevice PhysicalDevice() const { return physicalDevice; }
    const VkPhysicalDeviceProperties& PhysicalDeviceProperties() const { return physicalDeviceProperties; }
    const VkPhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties() const {
        return physicalDeviceMemoryProperties;
    }
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
    const VkColorSpaceKHR& AvailableSurfaceColorSpace(const uint32_t index) const {
        return availableSurfaceFormats[index].colorSpace;
    }
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
            outStream << std::format(
                "[ graphicsBase ] ERROR\nFailed to wait for the device to be idle!\nError code: "
                "{}\n",
                static_cast<int32_t>(result)
            );
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
            outStream << std::format(
                "[ graphicsBase ] ERROR\nFailed to create a vulkan instance!\nError code: {}\n",
                static_cast<uint32_t>(result)
            );
            return result;
        }
        if constexpr (ENABLE_DEBUG_MESSENGER) { CreateDebugMessenger(); }
        return VK_SUCCESS;
    }

    // 创建Vulkan实例失败后，检查所需的层是否可用
    static result_t CheckInstanceLayers(std::span<const char*> layersToCheck) {
        uint32_t layerCount = 0;
        std::vector<VkLayerProperties> availableLayers;
        if (const VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr)) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance layers!\n");
            return result;
        }

        if (layerCount) {
            availableLayers.resize(layerCount);
            if (const VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data())) {
                outStream << std::format(
                    "[ graphicsBase ] ERROR\nFailed to enumerate instance layer properties!\nError "
                    "code: {}\n",
                    static_cast<uint32_t>(result)
                );
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

    static result_t CheckInstanceExtensions(std::span<const char*> extensionsToCheck, const char* layerName = nullptr) {
        uint32_t extensionCount;
        std::vector<VkExtensionProperties> availableExtensions;
        if (const VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, nullptr)) {
            layerName
                ? outStream << std::format(
                      "[ graphicsBase ] ERROR\nFailed to get the count of instance "
                      "extensions!\nLayer name:{}\n",
                      layerName
                  )
                : outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\n");
            return result;
        }
        if (extensionCount) {
            availableExtensions.resize(extensionCount);
            if (const VkResult result =
                    vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, availableExtensions.data())) {
                outStream << std::format(
                    "[ graphicsBase ] ERROR\nFailed to enumerate instance extension "
                    "properties!\nError code: {}\n",
                    static_cast<uint32_t>(result)
                );
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
        if (VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr)) { return result; }
        if (!deviceCount) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to find any physical device supports vulkan!\n"),
                abort();
        }
        availablePhysicalDevices.resize(deviceCount);
        VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, availablePhysicalDevices.data());
        if (result) {
            outStream << std::format(
                "[ graphicsBase ] ERROR\nFailed to enumerate physical devices!\nError code: {}\n",
                static_cast<int32_t>(result)
            );
        }
        return result;
    }

    // 选择物理设备，并获得所需的队列族索引
    result_t DeterminePhysicalDevice(
        uint32_t deviceIndex = 0, bool enableGraphicsQueue = true, bool enableComputeQueue = true
    ) {
        // 定义一个特殊值用于标记一个队列簇索引已被找过但是为找到
        static constexpr uint32_t notFound = INT32_MAX;
        static std::vector<uint32_t> queueFamilyIndices(availablePhysicalDevices.size());

        if (queueFamilyIndices[deviceIndex] == notFound) { return VK_RESULT_MAX_ENUM; }

        // 如果队列簇索引被获取但还未被找到
        if (queueFamilyIndices[deviceIndex] == VK_QUEUE_FAMILY_IGNORED) {
            VkResult result = GetQueueFamilyIndices(availablePhysicalDevices[deviceIndex]);
            if (result) {
                if (result == VK_RESULT_MAX_ENUM) { queueFamilyIndices[deviceIndex] = notFound; }
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
            {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .pQueuePriorities = &queuePriority}
        };
        uint32_t queueCreateInfoCount = 0;
        if (queueFamilyIndex_graphics != VK_QUEUE_FAMILY_IGNORED) {
            queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_graphics;
        }
        if (queueFamilyIndex_presentation != VK_QUEUE_FAMILY_IGNORED &&
            queueFamilyIndex_presentation != queueFamilyIndex_graphics) {
            queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_presentation;
        }
        if (queueFamilyIndex_compute != VK_QUEUE_FAMILY_IGNORED &&
            queueFamilyIndex_compute != queueFamilyIndex_graphics &&
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
            outStream << std::format(
                "[ graphicsBase ] ERROR\nFailed to create a vulkan logical device!\nError code: "
                "{}\n",
                static_cast<int32_t>(result)
            );
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
        if (VkResult result =
                vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr)) {
            outStream << std::format(
                "[ graphicsBase ] ERROR\nFailed to get the count of surface formats!\nError code: "
                "{}\n",
                static_cast<int32_t>(result)
            );
            return result;
        }
        if (!surfaceFormatCount) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to find any supported surface format!\n"), abort();
        }
        availableSurfaceFormats.resize(surfaceFormatCount);
        VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice, surface, &surfaceFormatCount, availableSurfaceFormats.data()
        );
        if (result) {
            outStream << std::format(
                "[ graphicsBase ] ERROR\nFailed to get surface formats!\nError code: {}\n", static_cast<int32_t>(result)
            );
        }
        return result;
    }

    result_t SetSurfaceFormat(VkSurfaceFormatKHR surfaceFormat) {
        bool formatIsAvailable = false;
        if (!surfaceFormat.format) {
            // 如果格式未指定，只匹配色彩空间，图像格式则选择第一个可用格式
            for (auto& [format, colorSpace] : availableSurfaceFormats) {
                if (colorSpace == surfaceFormat.colorSpace) {
                    swapchainCreateInfo.imageFormat = format;
                    swapchainCreateInfo.imageColorSpace = colorSpace;
                    formatIsAvailable = true;
                    break;
                }
            }
        } else {
            // 否则匹配格式和色彩空间
            for (auto& [format, colorSpace] : availableSurfaceFormats) {
                if (format == surfaceFormat.format && colorSpace == surfaceFormat.colorSpace) {
                    swapchainCreateInfo.imageFormat = format;
                    swapchainCreateInfo.imageColorSpace = colorSpace;
                    formatIsAvailable = true;
                    break;
                }
            }
        }
        if (!formatIsAvailable) { return VK_ERROR_FORMAT_NOT_SUPPORTED; }
        // 如果交换链已创建，则重新创建交换链
        if (swapchain) { return RecreateSwapchain(); }
        return VK_SUCCESS;
    }

    // 创建交换链
    result_t CreateSwapchain(bool limitFrameRate = true, VkSwapchainCreateFlagsKHR flags = 0) {
        VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
        if (VkResult result =
                vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities)) {
            outStream << std::format(
                "[ graphicsBase ] ERROR\nFailed to get physical device surface "
                "capabilities!\nError code: {}\n",
                static_cast<int32_t>(result)
            );
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
                    swapchainCreateInfo.compositeAlpha = static_cast<VkCompositeAlphaFlagBitsKHR>(
                        surfaceCapabilities.supportedCompositeAlpha & (1 << i)
                    );
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
            outStream << std::format("[ graphicsBase ] WARNING\nVK_IMAGE_USAGE_TRANSFER_DST_BIT isn't supported!\n");
        }
        // 指定图像格式
        if (availableSurfaceFormats.empty()) {
            if (VkResult result = GetSurfaceFormats()) { return result; }
        }
        if (!swapchainCreateInfo.imageFormat) {
            if (SetSurfaceFormat({VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}) &&
                SetSurfaceFormat({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})) {
                swapchainCreateInfo.imageFormat = availableSurfaceFormats[0].format;
                swapchainCreateInfo.imageColorSpace = availableSurfaceFormats[0].colorSpace;
                outStream << std::format(
                    "[ graphicsBase ] WARNING\nFailed to select a four-component UNORM surface "
                    "format!\n"
                );
            }
        }

        // 指定呈现模式
        uint32_t surfacePresentModeCount;
        if (VkResult result =
                vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, nullptr)) {
            outStream << std::format(
                "[ graphicsBase ] ERROR\nFailed to get the count of surface present modes!\nError "
                "code: {}\n",
                static_cast<int32_t>(result)
            );
            return result;
        }
        if (!surfacePresentModeCount) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to find any surface present mode!\n"), abort();
        }
        std::vector<VkPresentModeKHR> surfacePresentModes(surfacePresentModeCount);
        if (VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(
                physicalDevice, surface, &surfacePresentModeCount, surfacePresentModes.data()
            )) {
            outStream << std::format(
                "[ graphicsBase ] ERROR\nFailed to get surface present modes!\nError code: {}\n",
                static_cast<int32_t>(result)
            );
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
        if (VkResult result = CreateSwapchain_Internal()) { return result; }
        ExecuteCallbacks(callbacks_createSwapchain);
        return VK_SUCCESS;
    }

    result_t RecreateSwapchain() {
        VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
        if (VkResult result =
                vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities)) {
            outStream << std::format(
                "[ graphicsBase ] ERROR\nFailed to get physical device surface "
                "capabilities!\nError code: {}\n",
                static_cast<int32_t>(result)
            );
            return result;
        }
        if (surfaceCapabilities.currentExtent.width == 0 || surfaceCapabilities.currentExtent.height == 0) {
            return VK_SUBOPTIMAL_KHR;
        }
        swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
        swapchainCreateInfo.oldSwapchain = swapchain;

        VkResult result = vkQueueWaitIdle(queue_graphics);
        // 仅在等待图形队列成功，且图形与呈现所用队列不同时等待呈现队列
        if (!result && queue_graphics != queue_presentation) { result = vkQueueWaitIdle(queue_presentation); }
        if (result) {
            outStream << std::format(
                "[ graphicsBase ] ERROR\nFailed to wait for the queue to be idle!\nError code: "
                "{}\n",
                static_cast<int32_t>(result)
            );
            return result;
        }
        ExecuteCallbacks(callbacks_destroySwapchain);
        // 销毁旧交换链图像视图
        for (auto& i : swapchainImageViews) {
            if (i) { vkDestroyImageView(device, i, nullptr); }
        }
        swapchainImageViews.resize(0);
        // 创建新交换链
        if ((result = CreateSwapchain_Internal())) { return result; }
        ExecuteCallbacks(callbacks_createSwapchain);
        return VK_SUCCESS;
    }

    // 该函数用于获取交换链图像索引到currentImageIndex
    result_t SwapImage(VkSemaphore semaphore_imageIsAvailable) {
        // 摧毁旧交换链
        if (swapchainCreateInfo.oldSwapchain && swapchainCreateInfo.oldSwapchain != swapchain) {
            vkDestroySwapchainKHR(device, swapchainCreateInfo.oldSwapchain, nullptr);
            swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
        }
        while (VkResult result = vkAcquireNextImageKHR(
                   device, swapchain, UINT64_MAX, semaphore_imageIsAvailable, VK_NULL_HANDLE, &currentImageIndex
               )) {
            switch (result) {
                case VK_SUBOPTIMAL_KHR:
                    // 交换链与Surface不兼容，不能用于渲染，必须重建交换链
                case VK_ERROR_OUT_OF_DATE_KHR:
                    // 重新创建交换链后继续获取交换链索引
                    if (VkResult recreateResult = RecreateSwapchain()) { return recreateResult; }
                    break;
                default:
                    outStream << std::format(
                        "[ graphicsBase ] ERROR\nFailed to acquire the next image!\nError code: "
                        "{}\n",
                        static_cast<int32_t>(result)
                    );
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
            outStream << std::format(
                "[ graphicsBase ] ERROR\nFailed to submit the command buffer!\nError code: {}\n",
                static_cast<int32_t>(result)
            );
        }
        return result;
    }

    // 渲染循环中提交命令缓冲区到图形队列
    result_t SubmitCommandBuffer_Graphics(
        VkCommandBuffer commandBuffer, VkSemaphore semaphore_imageIsAvailable = VK_NULL_HANDLE,
        VkSemaphore semaphore_renderingIsOver = VK_NULL_HANDLE, VkFence fence = VK_NULL_HANDLE,
        VkPipelineStageFlags waitDstStage_imageIsAvailable = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    ) const {
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
            outStream << std::format(
                "[ graphicsBase ] ERROR\nFailed to submit the command buffer!\nError code: {}\n",
                static_cast<int32_t>(result)
            );
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
                outStream << std::format(
                    "[ graphicsBase ] ERROR\nFailed to queue the image for presentation!\nError "
                    "code: {}\n",
                    static_cast<int32_t>(result)
                );
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
    explicit fence(VkFenceCreateInfo& createInfo) { Create(createInfo); }
    explicit fence(VkFenceCreateFlags flags = 0) { Create(flags); }
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
            outStream << std::format(
                "[ fence ] ERROR\nFailed to wait for the fence!\nError code: {}\n", static_cast<int32_t>(result)
            );
        }
        return result;
    }
    // CPU重置栅栏以便重用
    result_t Reset() const {
        VkResult result = vkResetFences(graphicsBase::Base().Device(), 1, &handle);
        if (result) {
            outStream << std::format(
                "[ fence ] ERROR\nFailed to reset the fence!\nError code: {}\n", static_cast<int32_t>(result)
            );
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
            outStream << std::format(
                "[ fence ] ERROR\nFailed to get the status of the fence!\nError code: {}\n",
                static_cast<int32_t>(result)
            );
        }
        return result;
    }
    // Non-const function
    result_t Create(VkFenceCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkResult result = vkCreateFence(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result) {
            outStream << std::format(
                "[ fence ] ERROR\nFailed to create a fence!\nError code: {}\n", static_cast<int32_t>(result)
            );
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
    explicit semaphore(VkSemaphoreCreateInfo& createInfo) { Create(createInfo); }
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
            outStream << std::format(
                "[ semaphore ] ERROR\nFailed to create a semaphore!\nError code: {}\n", static_cast<int32_t>(result)
            );
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
            outStream << std::format(
                "[ commandBuffer ] ERROR\nFailed to begin a command buffer!\nError code: {}\n",
                static_cast<int32_t>(result)
            );
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
            outStream << std::format(
                "[ commandBuffer ] ERROR\nFailed to begin a command buffer!\nError code: {}\n",
                static_cast<int32_t>(result)
            );
        }
        return result;
    }
    result_t End() const {
        VkResult result = vkEndCommandBuffer(handle);
        if (result) {
            outStream << std::format(
                "[ commandBuffer ] ERROR\nFailed to end a command buffer!\nError code: {}\n",
                static_cast<int32_t>(result)
            );
        }
        return result;
    }
};

class commandPool {
    VkCommandPool handle = VK_NULL_HANDLE;

  public:
    commandPool() = default;
    explicit commandPool(VkCommandPoolCreateInfo& createInfo) { Create(createInfo); }
    explicit commandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0) {
        Create(queueFamilyIndex, flags);
    }
    commandPool(commandPool&& other) noexcept { MoveHandle; }
    ~commandPool() { DestroyHandleBy(vkDestroyCommandPool); }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    // Const function
    result_t AllocateBuffers(
        arrayRef<VkCommandBuffer> buffers, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
    ) const {
        VkCommandBufferAllocateInfo allocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = handle,
            .level = level,
            .commandBufferCount = static_cast<uint32_t>(buffers.Count())
        };
        VkResult result = vkAllocateCommandBuffers(graphicsBase::Base().Device(), &allocateInfo, buffers.Pointer());
        if (result) {
            outStream << std::format(
                "[ commandPool ] ERROR\nFailed to allocate command buffers!\nError code: {}\n",
                static_cast<int32_t>(result)
            );
        }
        return result;
    }
    result_t AllocateBuffers(
        arrayRef<commandBuffer> buffers, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
    ) const {
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
            outStream << std::format(
                "[ commandPool ] ERROR\nFailed to create a command pool!\nError code: {}\n",
                static_cast<int32_t>(result)
            );
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
    explicit renderPass(VkRenderPassCreateInfo& createInfo) { Create(createInfo); }
    renderPass(renderPass&& other) noexcept { MoveHandle; }
    ~renderPass() { DestroyHandleBy(vkDestroyRenderPass); }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    // Const function
    void CmdBegin(
        VkCommandBuffer commandBuffer, VkRenderPassBeginInfo& beginInfo,
        VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE
    ) const {
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.renderPass = handle;
        vkCmdBeginRenderPass(commandBuffer, &beginInfo, subpassContents);
    }
    void CmdBegin(
        VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, VkRect2D renderArea,
        arrayRef<const VkClearValue> clearValues = {}, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE
    ) const {
        VkRenderPassBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = handle,
            .framebuffer = framebuffer,
            .renderArea = renderArea,
            .clearValueCount = static_cast<uint32_t>(clearValues.Count()),
            .pClearValues = clearValues.Pointer()
        };
        vkCmdBeginRenderPass(commandBuffer, &beginInfo, subpassContents);
    }
    static void CmdNext(VkCommandBuffer commandBuffer, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) {
        vkCmdNextSubpass(commandBuffer, subpassContents);
    }
    static void CmdEnd(VkCommandBuffer commandBuffer) { vkCmdEndRenderPass(commandBuffer); }
    // Non-const function
    result_t Create(VkRenderPassCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        VkResult result = vkCreateRenderPass(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result) {
            outStream << std::format(
                "[ renderPass ] ERROR\nFailed to create a render pass!\nError code: {}\n", static_cast<int32_t>(result)
            );
        }
        return result;
    }
};

class framebuffer {
    VkFramebuffer handle = VK_NULL_HANDLE;

  public:
    framebuffer() = default;
    explicit framebuffer(VkFramebufferCreateInfo& createInfo) { Create(createInfo); }
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
            outStream << std::format(
                "[ framebuffer ] ERROR\nFailed to create a framebuffer!\nError code: {}\n", static_cast<int32_t>(result)
            );
        }
        return result;
    }
};

class shaderModule {
    VkShaderModule handle = VK_NULL_HANDLE;

  public:
    shaderModule() = default;
    explicit shaderModule(VkShaderModuleCreateInfo& createInfo) { Create(createInfo); }
    explicit shaderModule(const char* filepath /*VkShaderModuleCreateFlags flags*/) { Create(filepath); }
    shaderModule(size_t codeSize, const uint32_t* pCode /*VkShaderModuleCreateFlags flags*/) {
        Create(codeSize, pCode);
    }
    shaderModule(shaderModule&& other) noexcept { MoveHandle; }
    ~shaderModule() { DestroyHandleBy(vkDestroyShaderModule); }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    // const function
    VkPipelineShaderStageCreateInfo StageCreateInfo(VkShaderStageFlagBits stage, const char* entry = "main") const {
        return {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,  // sType
            nullptr,                                              // pNext
            0,                                                    // flags
            stage,                                                // stage
            handle,                                               // module
            entry,                                                // pName
            nullptr                                               // pSpecializationInfo
        };
    }
    // Non-const function
    result_t Create(VkShaderModuleCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        VkResult result = vkCreateShaderModule(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result) {
            outStream << std::format(
                "[ shader ] ERROR\nFailed to create a shader module!\nError code: {}\n", static_cast<int32_t>(result)
            );
        }
        return result;
    }
    result_t Create(const char* filepath /*VkShaderModuleCreateFlags flags*/) {
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);
        if (!file) {
            outStream << std::format("[ shader ] ERROR\nFailed to open the file: {}\n", filepath);
            return VK_RESULT_MAX_ENUM;
        }
        size_t fileSize = size_t(file.tellg());
        std::vector<uint32_t> binaries(fileSize / 4);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(binaries.data()), fileSize);
        file.close();
        return Create(fileSize, binaries.data());
    }
    result_t Create(size_t codeSize, const uint32_t* pCode /*VkShaderModuleCreateFlags flags*/) {
        VkShaderModuleCreateInfo createInfo = {
            .codeSize = codeSize,
            .pCode = pCode,
        };
        return Create(createInfo);
    }
};

class pipelineLayout {
    VkPipelineLayout handle = VK_NULL_HANDLE;

  public:
    pipelineLayout() = default;
    pipelineLayout(VkPipelineLayoutCreateInfo& createInfo) { Create(createInfo); }
    pipelineLayout(pipelineLayout&& other) noexcept { MoveHandle; }
    ~pipelineLayout() { DestroyHandleBy(vkDestroyPipelineLayout); }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    // Non-const function
    result_t Create(VkPipelineLayoutCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        VkResult result = vkCreatePipelineLayout(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result) {
            outStream << std::format(
                "[ pipelineLayout ] ERROR\nFailed to create a pipeline layout!\nError code: {}\n",
                static_cast<int32_t>(result)
            );
        }
        return result;
    }
};

class pipeline {
    VkPipeline handle = VK_NULL_HANDLE;

  public:
    pipeline() = default;
    explicit pipeline(VkGraphicsPipelineCreateInfo& createInfo) { Create(createInfo); }
    explicit pipeline(VkComputePipelineCreateInfo& createInfo) { Create(createInfo); }
    pipeline(pipeline&& other) noexcept { MoveHandle; }
    ~pipeline() { DestroyHandleBy(vkDestroyPipeline); }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    // Non-const function
    result_t Create(VkGraphicsPipelineCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        VkResult result =
            vkCreateGraphicsPipelines(graphicsBase::Base().Device(), VK_NULL_HANDLE, 1, &createInfo, nullptr, &handle);
        if (result) {
            outStream << std::format(
                "[ pipeline ] ERROR\nFailed to create a graphics pipeline!\nError code: {}\n",
                static_cast<int32_t>(result)
            );
        }
        return result;
    }
    result_t Create(VkComputePipelineCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        VkResult result =
            vkCreateComputePipelines(graphicsBase::Base().Device(), VK_NULL_HANDLE, 1, &createInfo, nullptr, &handle);
        if (result) {
            outStream << std::format(
                "[ pipeline ] ERROR\nFailed to create a compute pipeline!\nError code: {}\n",
                static_cast<int32_t>(result)
            );
        }
        return result;
    }
};

// 设备内存的封装类
class deviceMemory {
    VkDeviceMemory handle = VK_NULL_HANDLE;
    VkDeviceSize allocationSize = 0;             // 实际会分配的内存大小
    VkMemoryPropertyFlags memoryProperties = 0;  // 内存属性

    /**
     * 调整非一致内存访问（non-coherent memory）的映射范围，Vulkan要求必须以固定字节对齐的方式访问
     * 非一致内存：某些GPU内存在CPU和GPU之间的数据同步需要特殊处理
     * params：
     *  size:希望映射的原始大小
     *  offset: 希望映射的原始偏移
     */
    VkDeviceSize AdjustNonCoherentMemoryRange(VkDeviceSize& size, VkDeviceSize& offset) const {
        // nonCoherentAtomSize: Vulkan 要求的对齐粒度
        const VkDeviceSize& nonCoherentAtomSize =
            graphicsBase::Base().PhysicalDeviceProperties().limits.nonCoherentAtomSize;
        // 保存原始offset
        VkDeviceSize _offset = offset;
        // 计算原始映射范围的结束位置
        VkDeviceSize rangeEnd = size + offset;
        // 将offset向下对齐，rangeEnd向上对齐
        offset = offset / nonCoherentAtomSize * nonCoherentAtomSize;
        rangeEnd = (rangeEnd + nonCoherentAtomSize - 1) / nonCoherentAtomSize * nonCoherentAtomSize;
        // 确保rangeEnd不超过实际分配的内存大小
        rangeEnd = std::min(rangeEnd, allocationSize);
        // 重新计算size（现在offset和rangeEnd都满足对齐要求）
        size = rangeEnd - offset;
        // 返回调整后的偏移量差值
        return _offset - offset;
    }

  protected:
    // 用于bufferMemory或imageMemory,定义于此能节省八个字节
    class {
        friend class bufferMemory;
        friend class imageMemory;
        bool value = false;
        explicit operator bool() const { return value; }
        auto& operator=(bool value) {
            this->value = value;
            return *this;
        }
    } areBound;

  public:
    deviceMemory() = default;
    explicit deviceMemory(VkMemoryAllocateInfo& allocateInfo) { Allocate(allocateInfo); }
    deviceMemory(deviceMemory&& other) noexcept {
        MoveHandle;
        allocationSize = other.allocationSize;
        memoryProperties = other.memoryProperties;
        other.allocationSize = 0;
        other.memoryProperties = 0;
    }
    ~deviceMemory() {
        DestroyHandleBy(vkFreeMemory);
        allocationSize = 0;
        memoryProperties = 0;
    }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    VkDeviceSize AllocationSize() const { return allocationSize; }
    VkMemoryPropertyFlags MemoryProperties() const { return memoryProperties; }
    // Const function
    // 映射host visible的内存区,在对其进行映射（map）后，可以由CPU侧对其进行直接读写
    result_t MapMemory(void*& pData, VkDeviceSize size, VkDeviceSize offset = 0) const {
        VkDeviceSize inverseDeltaOffset{};
        if (!(memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            inverseDeltaOffset = AdjustNonCoherentMemoryRange(size, offset);
        }
        if (VkResult result = vkMapMemory(graphicsBase::Base().Device(), handle, offset, size, 0, &pData)) {
            outStream << std::format(
                "[ deviceMemory ] ERROR\nFailed to map the memory!\nError code: {}\n", int32_t(result)
            );
            return result;
        }
        if (!(memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            pData = static_cast<uint8_t*>(pData) + inverseDeltaOffset;
            VkMappedMemoryRange mappedMemoryRange = {
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .memory = handle, .offset = offset, .size = size
            };
            // 确保物理设备对该片内存的写入可以被CPU侧正确读取
            if (VkResult result =
                    vkInvalidateMappedMemoryRanges(graphicsBase::Base().Device(), 1, &mappedMemoryRange)) {
                outStream << std::format(
                    "[ deviceMemory ] ERROR\nFailed to invalidate the mapped memory range!\nError "
                    "code: {}\n",
                    static_cast<int32_t>(result)
                );
                return result;
            }
        }
        return VK_SUCCESS;
    }

    // 取消映射host visible的内存区
    result_t UnmapMemory(VkDeviceSize size, VkDeviceSize offset = 0) const {
        if (!(memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            AdjustNonCoherentMemoryRange(size, offset);
            VkMappedMemoryRange mappedMemoryRange = {
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .memory = handle, .offset = offset, .size = size
            };
            if (VkResult result = vkFlushMappedMemoryRanges(graphicsBase::Base().Device(), 1, &mappedMemoryRange)) {
                outStream << std::format(
                    "[ deviceMemory ] ERROR\nFailed to flush the memory!\nError code: {}\n",
                    static_cast<int32_t>(result)
                );
                return result;
            }
        }
        vkUnmapMemory(graphicsBase::Base().Device(), handle);
        return VK_SUCCESS;
    }

    /**
     * 将CPU内存中的数据复制到GPU的缓冲区需要先映射设备内存，然后使用memcpy进行数据传输，最后取消映射设备内存
     * pData_src：指向源数据的指针,CPU端
     * offset：GPU缓冲区的偏移位置
     */
    result_t BufferData(const void* pData_src, VkDeviceSize size, VkDeviceSize offset = 0) const {
        void* pData_dst;
        if (VkResult result = MapMemory(pData_dst, size, offset)) { return result; }
        memcpy(pData_dst, pData_src, static_cast<size_t>(size));
        return UnmapMemory(size, offset);
    }
    result_t BufferData(const auto& data_src) const { return BufferData(&data_src, sizeof data_src); }

    // RetrieveData(...)用于从设备内存区读取数据，适用于memcpy从内存区读取数据后立刻取消映射的情况
    result_t RetrieveData(void* pData_dst, VkDeviceSize size, VkDeviceSize offset = 0) const {
        void* pData_src;
        if (VkResult result = MapMemory(pData_src, size, offset)) { return result; }
        memcpy(pData_dst, pData_src, static_cast<size_t>(size));
        return UnmapMemory(size, offset);
    }

    // Non-const function
    result_t Allocate(VkMemoryAllocateInfo& allocateInfo) {
        if (allocateInfo.memoryTypeIndex >= graphicsBase::Base().PhysicalDeviceMemoryProperties().memoryTypeCount) {
            outStream << std::format("[ deviceMemory ] ERROR\nInvalid memory type index!\n");
            return VK_RESULT_MAX_ENUM;
        }
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        if (VkResult result = vkAllocateMemory(graphicsBase::Base().Device(), &allocateInfo, nullptr, &handle)) {
            outStream << std::format(
                "[ deviceMemory ] ERROR\nFailed to allocate device memory!\nError code: {}\n",
                static_cast<int32_t>(result)
            );
            return result;
        }
        // 记录实际分配的内存大小和内存属性
        allocationSize = allocateInfo.allocationSize;
        memoryProperties = graphicsBase::Base()
                               .PhysicalDeviceMemoryProperties()
                               .memoryTypes[allocateInfo.memoryTypeIndex]
                               .propertyFlags;
        return VK_SUCCESS;
    }
};

// 缓冲区类,引用设备内存，指代缓冲区数据
class buffer {
    VkBuffer handle = VK_NULL_HANDLE;

  public:
    buffer() = default;
    explicit buffer(VkBufferCreateInfo& createInfo) { Create(createInfo); }
    buffer(buffer&& other) noexcept { MoveHandle; }
    ~buffer() { DestroyHandleBy(vkDestroyBuffer); }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    // Const function
    VkMemoryAllocateInfo MemoryAllocateInfo(VkMemoryPropertyFlags desiredMemoryProperties) const {
        VkMemoryAllocateInfo memoryAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        };
        // 或缺缓冲区内存分配要求
        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(graphicsBase::Base().Device(), handle, &memoryRequirements);
        memoryAllocateInfo.allocationSize = memoryRequirements.size;
        auto& physicalDeviceMemoryProperties = graphicsBase::Base().PhysicalDeviceMemoryProperties();
        for (size_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {
            // 如果相应的设备内存类型支持该缓冲区，则继续执行后续判断，否则短路
            if (memoryRequirements.memoryTypeBits & 1 << i &&
                // 如果相应的设备内存类型支持所需的内存属性，覆写memoryAllocateInfo.memoryTypeIndex
                (physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & desiredMemoryProperties) ==
                    desiredMemoryProperties) {
                memoryAllocateInfo.memoryTypeIndex = i;
                break;
            }
        }
        // 交由外部处理是否成功取得内存返回类型索引
        return memoryAllocateInfo;
    }
    result_t BindMemory(VkDeviceMemory deviceMemory, VkDeviceSize memoryOffset = 0) const {
        VkResult result = vkBindBufferMemory(graphicsBase::Base().Device(), handle, deviceMemory, memoryOffset);
        if (result) {
            outStream << std::format(
                "[ buffer ] ERROR\nFailed to attach the memory!\nError code: {}\n", static_cast<int32_t>(result)
            );
        }
        return result;
    }
    // Non-const function
    result_t Create(VkBufferCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        VkResult result = vkCreateBuffer(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result) {
            outStream << std::format(
                "[ buffer ] ERROR\nFailed to create a buffer!\nError code: {}\n", static_cast<int32_t>(result)
            );
        }
        return result;
    }
};

class bufferMemory : buffer, deviceMemory {
  public:
    bufferMemory() = default;
    bufferMemory(VkBufferCreateInfo& createInfo, VkMemoryPropertyFlags desiredMemoryProperties) {
        Create(createInfo, desiredMemoryProperties);
    }
    bufferMemory(bufferMemory&& other) noexcept : buffer(std::move(other)), deviceMemory(std::move(other)) {
        areBound = other.areBound;
        other.areBound = false;
    }
    ~bufferMemory() { areBound = false; }
    // Getter
    VkBuffer Buffer() const { return static_cast<const buffer&>(*this); }
    const VkBuffer* AddressOfBuffer() const { return buffer::Address(); }
    VkDeviceMemory Memory() const { return static_cast<const deviceMemory&>(*this); }
    const VkDeviceMemory* AddressOfMemory() const { return deviceMemory::Address(); }
    // 若areBound 为true则表示成功分配了设备内存、创建了缓冲区且成功绑定
    auto AreBound() const { return areBound; }
    using deviceMemory::AllocationSize;
    using deviceMemory::MemoryProperties;
    // Const function
    using deviceMemory::BufferData;
    using deviceMemory::MapMemory;
    using deviceMemory::RetrieveData;
    using deviceMemory::UnmapMemory;

    // Non-const function
    result_t CreateBuffer(VkBufferCreateInfo& createInfo) { return buffer::Create(createInfo); }

    result_t AllocateMemory(VkMemoryPropertyFlags desiredMemoryProperties) {
        VkMemoryAllocateInfo allocateInfo = MemoryAllocateInfo(desiredMemoryProperties);
        if (allocateInfo.memoryTypeIndex >= graphicsBase::Base().PhysicalDeviceMemoryProperties().memoryTypeCount) {
            return VK_RESULT_MAX_ENUM;
        }
        return Allocate(allocateInfo);
    }

    result_t BindMemory() {
        if (VkResult result = buffer::BindMemory(Memory())) { return result; }
        areBound = true;
        return VK_SUCCESS;
    }

    // 分配设备内存、创建缓冲区并绑定内存
    result_t Create(VkBufferCreateInfo& createInfo, VkMemoryPropertyFlags desiredMemoryProperties) {
        VkResult result;
        false || (result = CreateBuffer(createInfo)) || (result = AllocateMemory(desiredMemoryProperties)) ||
            (result = BindMemory());
        return result;
    }
};

class bufferView {
    VkBufferView handle = VK_NULL_HANDLE;

  public:
    bufferView() = default;
    explicit bufferView(VkBufferViewCreateInfo& createInfo) { Create(createInfo); }
    bufferView(VkBuffer buffer, VkFormat format, VkDeviceSize offset = 0, VkDeviceSize range = 0) {
        Create(buffer, format, offset, range);
    }
    bufferView(bufferView&& other) noexcept { MoveHandle; }
    ~bufferView() { DestroyHandleBy(vkDestroyBufferView); }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    // Non-const function
    result_t Create(VkBufferViewCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        VkResult result = vkCreateBufferView(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result) {
            outStream << std::format(
                "[ bufferView ] ERROR\nFailed to create a buffer view!\nError code: {}\n", int32_t(result)
            );
        }
        return result;
    }

    result_t Create(VkBuffer buffer, VkFormat format, VkDeviceSize offset = 0, VkDeviceSize range = 0) {
        VkBufferViewCreateInfo createInfo = {
            .buffer = buffer,
            .format = format,
            .offset = offset,
            .range = range,
        };
        return Create(createInfo);
    }
};

class image {
    VkImage handle = VK_NULL_HANDLE;

  public:
    image() = default;
    explicit image(VkImageCreateInfo& createInfo) { Create(createInfo); }
    image(image&& other) noexcept { MoveHandle; }
    ~image() { DestroyHandleBy(vkDestroyImage); }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    // Const function
    VkMemoryAllocateInfo MemoryAllocateInfo(VkMemoryPropertyFlags desiredMemoryProperties) const {
        VkMemoryAllocateInfo memoryAllocateInfo = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        VkMemoryRequirements memoryRequirements;
        vkGetImageMemoryRequirements(graphicsBase::Base().Device(), handle, &memoryRequirements);
        memoryAllocateInfo.allocationSize = memoryRequirements.size;
        auto GetMemoryTypeIndex = [](uint32_t memoryTypeBits, VkMemoryPropertyFlags desiredMemoryProperties) {
            auto& physicalDeviceMemoryProperties = graphicsBase::Base().PhysicalDeviceMemoryProperties();
            for (size_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {
                if (memoryTypeBits & 1 << i && (physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags &
                                                desiredMemoryProperties) == desiredMemoryProperties) {
                    return static_cast<uint32_t>(i);
                }
            }
            return UINT32_MAX;
        };
        memoryAllocateInfo.memoryTypeIndex =
            GetMemoryTypeIndex(memoryRequirements.memoryTypeBits, desiredMemoryProperties);
        if (memoryAllocateInfo.memoryTypeIndex == UINT32_MAX &&
            desiredMemoryProperties & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) {
            memoryAllocateInfo.memoryTypeIndex = GetMemoryTypeIndex(
                memoryRequirements.memoryTypeBits, desiredMemoryProperties & ~VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT
            );
        }
        return memoryAllocateInfo;
    }

    result_t BindMemory(VkDeviceMemory deviceMemory, VkDeviceSize memoryOffset = 0) const {
        VkResult result = vkBindImageMemory(graphicsBase::Base().Device(), handle, deviceMemory, memoryOffset);
        if (result) {
            outStream << std::format(
                "[ image ] ERROR\nFailed to attach the memory!\nError code: {}\n", static_cast<int32_t>(result)
            );
        }
        return result;
    }

    result_t Create(VkImageCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        VkResult result = vkCreateImage(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result) {
            outStream << std::format(
                "[ image ] ERROR\nFailed to create an image!\nError code: {}\n", static_cast<int32_t>(result)
            );
        }
        return result;
    }
};

class imageMemory : image, deviceMemory {
  public:
    imageMemory() = default;
    imageMemory(VkImageCreateInfo& createInfo, VkMemoryPropertyFlags desiredMemoryProperties) {}
    imageMemory(imageMemory&& other) noexcept : image(std::move(other)), deviceMemory(std::move(other)) {
        areBound = other.areBound;
        other.areBound = false;
    }
    ~imageMemory() { areBound = false; }
    // Getter
    VkImage Image() const { return static_cast<const image&>(*this); }
    const VkImage* AddressOfImage() const { return image::Address(); }
    VkDeviceMemory Memory() const { return static_cast<const deviceMemory&>(*this); }
    const VkDeviceMemory* AddressOfMemory() const { return deviceMemory::Address(); }
    auto AreBound() const { return areBound; }
    using deviceMemory::AllocationSize;
    using deviceMemory::MemoryProperties;
    // Non-const function
    result_t CreateImage(VkImageCreateInfo& createInfo) { return image::Create(createInfo); }

    result_t AllocateMemory(VkMemoryPropertyFlags desiredMemoryProperties) {
        VkMemoryAllocateInfo allocateInfo = MemoryAllocateInfo(desiredMemoryProperties);
        if (allocateInfo.memoryTypeIndex >= graphicsBase::Base().PhysicalDeviceMemoryProperties().memoryTypeCount) {
            return VK_RESULT_MAX_ENUM;
        }
        return Allocate(allocateInfo);
    }

    result_t BindMemory() {
        if (VkResult result = image::BindMemory(Memory())) { return result; }
        areBound = true;
        return VK_SUCCESS;
    }

    result_t Create(VkImageCreateInfo& createInfo, VkMemoryPropertyFlags desiredMemoryProperties) {
        VkResult result;
        false || (result = CreateImage(createInfo)) || (result = AllocateMemory(desiredMemoryProperties)) ||
            (result = BindMemory());
        return result;
    }
};

class imageView {
    VkImageView handle = VK_NULL_HANDLE;

  public:
    imageView() = default;
    explicit imageView(VkImageViewCreateInfo& createInfo) { Create(createInfo); }
    imageView(
        VkImage image, VkImageViewType viewType, VkFormat format, const VkImageSubresourceRange& subresourceRange,
        VkImageViewCreateFlags flags = 0
    ) {
        Create(image, viewType, format, subresourceRange, flags);
    }
    imageView(imageView&& other) noexcept { MoveHandle; }
    ~imageView() { DestroyHandleBy(vkDestroyImageView); }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    // Non-const function
    result_t Create(VkImageViewCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        VkResult result = vkCreateImageView(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result) {
            outStream << std::format(
                "[ imageView ] ERROR\nFailed to create an image view!\nError code: {}\n", static_cast<int32_t>(result)
            );
        }
        return result;
    }
    result_t Create(
        VkImage image, VkImageViewType viewType, VkFormat format, const VkImageSubresourceRange& subresourceRange,
        VkImageViewCreateFlags flags = 0
    ) {
        VkImageViewCreateInfo createInfo = {
            .flags = flags,
            .image = image,
            .viewType = viewType,
            .format = format,
            .subresourceRange = subresourceRange,
        };
        return Create(createInfo);
    }
};

class descriptorSetLayout {
    VkDescriptorSetLayout handle = VK_NULL_HANDLE;

  public:
    descriptorSetLayout() = default;
    explicit descriptorSetLayout(VkDescriptorSetLayoutCreateInfo& createInfo) { Create(createInfo); }
    descriptorSetLayout(descriptorSetLayout&& other) noexcept { MoveHandle; }
    ~descriptorSetLayout() { DestroyHandleBy(vkDestroyDescriptorSetLayout); }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    // Non-const function
    result_t Create(VkDescriptorSetLayoutCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        VkResult result = vkCreateDescriptorSetLayout(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result) {
            outStream << std::format(
                "[ descriptorSetLayout ] ERROR\nFailed to create a descriptor set layout!\nError code: {}\n",
                int32_t(result)
            );
        }
        return result;
    }
};

class descriptorSet {
    friend class descriptorPool;
    VkDescriptorSet handle = VK_NULL_HANDLE;

  public:
    descriptorSet() = default;
    descriptorSet(descriptorSet&& other) noexcept : handle(other.handle) { MoveHandle; }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    // Const function
    void Write(
        arrayRef<const VkDescriptorImageInfo> descriptorInfos, VkDescriptorType descriptorType, uint32_t dstBinding = 0,
        uint32_t dstArrayElement = 0
    ) const {
        VkWriteDescriptorSet writeDescriptorSet = {
            .dstSet = handle,
            .dstBinding = dstBinding,
            .dstArrayElement = dstArrayElement,
            .descriptorCount = static_cast<uint32_t>(descriptorInfos.Count()),
            .descriptorType = descriptorType,
            .pImageInfo = descriptorInfos.Pointer(),
        };
        Update(arrayRef(writeDescriptorSet));
    }
    void Write(
        arrayRef<const VkDescriptorBufferInfo> descriptorInfos, VkDescriptorType descriptorType,
        uint32_t dstBinding = 0, uint32_t dstArrayElement = 0
    ) const {
        VkWriteDescriptorSet writeDescriptorSet = {
            .dstSet = handle,
            .dstBinding = dstBinding,
            .dstArrayElement = dstArrayElement,
            .descriptorCount = static_cast<uint32_t>(descriptorInfos.Count()),
            .descriptorType = descriptorType,
            .pBufferInfo = descriptorInfos.Pointer(),
        };
        Update(arrayRef(writeDescriptorSet));
    }
    void Write(
        arrayRef<const VkBufferView> descriptorInfos, VkDescriptorType descriptorType, uint32_t dstBinding = 0,
        uint32_t dstArrayElement = 0
    ) const {
        VkWriteDescriptorSet writeDescriptorSet = {
            .dstSet = handle,
            .dstBinding = dstBinding,
            .dstArrayElement = dstArrayElement,
            .descriptorCount = static_cast<uint32_t>(descriptorInfos.Count()),
            .descriptorType = descriptorType,
            .pTexelBufferView = descriptorInfos.Pointer(),
        };
        Update(arrayRef(writeDescriptorSet));
    }
    void Write(
        arrayRef<const bufferView> descriptorInfos, VkDescriptorType descriptorType, uint32_t dstBinding = 0,
        uint32_t dstArrayElement = 0
    ) const {
        Write({descriptorInfos[0].Address(), descriptorInfos.Count()}, descriptorType, dstBinding, dstArrayElement);
    }
    // Static function
    static void Update(arrayRef<VkWriteDescriptorSet> writes, arrayRef<VkCopyDescriptorSet> copies = {}) {
        for (auto& i : writes) { i.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; }
        for (auto& i : copies) { i.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET; }
        vkUpdateDescriptorSets(
            graphicsBase::Base().Device(), writes.Count(), writes.Pointer(), copies.Count(), copies.Pointer()
        );
    }
};

class descriptorPool {
    VkDescriptorPool handle = VK_NULL_HANDLE;

  public:
    descriptorPool() = default;
    explicit descriptorPool(VkDescriptorPoolCreateInfo& createInfo) { Create(createInfo); }
    explicit descriptorPool(
        uint32_t maxSetCount, arrayRef<const VkDescriptorPoolSize> poolSizes, VkDescriptorPoolCreateFlags flags = 0
    ) {
        Create(maxSetCount, poolSizes, flags);
    }
    descriptorPool(descriptorPool&& other) noexcept { MoveHandle; }
    ~descriptorPool() { DestroyHandleBy(vkDestroyDescriptorPool); }
    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    // Const function
    result_t AllocateSets(arrayRef<VkDescriptorSet> sets, arrayRef<const VkDescriptorSetLayout> setLayouts) const {
        if (sets.Count() != setLayouts.Count()) {
            if (sets.Count() < setLayouts.Count()) {
                outStream << std::format(
                    "[ descriptorPool ] ERROR\nFor each descriptor set, must provide a corresponding layout!\n"
                );
                return VK_RESULT_MAX_ENUM;  // 没有合适的错误代码，别用VK_ERROR_UNKNOWN
            } else {
                outStream << std::format("[ descriptorPool ] WARNING\nProvided layouts are more than sets!\n");
            }
        }
        VkDescriptorSetAllocateInfo allocateInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = handle,
            .descriptorSetCount = static_cast<uint32_t>(sets.Count()),
            .pSetLayouts = setLayouts.Pointer()
        };
        VkResult result = vkAllocateDescriptorSets(graphicsBase::Base().Device(), &allocateInfo, sets.Pointer());
        if (result) {
            outStream << std::format(
                "[ descriptorPool ] ERROR\nFailed to allocate descriptor sets!\nError code: {}\n", int32_t(result)
            );
        }
        return result;
    }
    result_t AllocateSets(arrayRef<VkDescriptorSet> sets, arrayRef<const descriptorSetLayout> setLayouts) const {
        return AllocateSets(
            arrayRef<VkDescriptorSet>(sets),
            arrayRef<const VkDescriptorSetLayout>{setLayouts[0].Address(), setLayouts.Count()}
        );
    }
    result_t AllocateSets(arrayRef<descriptorSet> sets, arrayRef<const VkDescriptorSetLayout> setLayouts) const {
        return AllocateSets(
            arrayRef<VkDescriptorSet>{&sets[0].handle, sets.Count()}, arrayRef<const VkDescriptorSetLayout>(setLayouts)
        );
    }
    result_t AllocateSets(arrayRef<descriptorSet> sets, arrayRef<const descriptorSetLayout> setLayouts) const {
        return AllocateSets(
            arrayRef<VkDescriptorSet>{&sets[0].handle, sets.Count()},
            arrayRef<const VkDescriptorSetLayout>{setLayouts[0].Address(), setLayouts.Count()}
        );
    }
    result_t FreeSets(arrayRef<VkDescriptorSet> sets) const {
        VkResult result = vkFreeDescriptorSets(graphicsBase::Base().Device(), handle, sets.Count(), sets.Pointer());
        memset(sets.Pointer(), 0, sets.Count() * sizeof(VkDescriptorSet));
        return result;  // Though vkFreeDescriptorSets(...) can only return VK_SUCCESS
    }
    result_t FreeSets(arrayRef<descriptorSet> sets) const { return FreeSets({&sets[0].handle, sets.Count()}); }
    // Non-const function
    result_t Create(VkDescriptorPoolCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        VkResult result = vkCreateDescriptorPool(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result)
            outStream << std::format(
                "[ descriptorPool ] ERROR\nFailed to create a descriptor pool!\nError code: {}\n", int32_t(result)
            );
        return result;
    }
    result_t Create(
        uint32_t maxSetCount, arrayRef<const VkDescriptorPoolSize> poolSizes, VkDescriptorPoolCreateFlags flags = 0
    ) {
        VkDescriptorPoolCreateInfo createInfo = {
            .flags = flags,
            .maxSets = maxSetCount,
            .poolSizeCount = uint32_t(poolSizes.Count()),
            .pPoolSizes = poolSizes.Pointer()
        };
        return Create(createInfo);
    }
};

}  // namespace vulkan
