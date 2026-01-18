#pragma once

#include "VKBase.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// 窗口的指针
inline GLFWwindow* pWindow;
// 显示器信息的指针
inline GLFWmonitor* pMonitor;
// 窗口标题
inline auto windowTitle = "EasyVK";

inline bool InitializeWindow(
    VkExtent2D size, bool fullScreen = false, bool isResizable = true, bool limitFrameRate = true
) {
    using namespace vulkan;
    if (!glfwInit()) {
        std::cout << std::format("[ InitializeWindow ] ERROR\nFailed to initialize GLFW!\n");
        return false;
    }

    // 不创建OpenGL上下文
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // 是否允许调整窗口大小
    glfwWindowHint(GLFW_RESIZABLE, isResizable);
    // 当前显示器的指针
    pMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);
    pWindow = fullScreen
                  ? glfwCreateWindow(pMode->width, pMode->height, windowTitle, pMonitor, nullptr)
                  : glfwCreateWindow(size.width, size.height, windowTitle, nullptr, nullptr);

    if (!pWindow) {
        std::cout << std::format("[ InitializeWindow ]\nFailed to create a glfw window!\n");
        glfwTerminate();
        return false;
    }

    // 添加所需的实例扩展
#ifdef _WIN32
    graphicsBase::Base().AddInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME);
    graphicsBase::Base().AddInstanceExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
    uint32_t extensionCount = 0;
    const char** extensionNames;
    extensionNames = glfwGetRequiredInstanceExtensions(&extensionCount);
    for (size_t i = 0; i < extensionCount; i++) {
        graphicsBase::Base().AddInstanceExtension(extensionNames[i]);
    }
#endif
    // 添加所需的设备扩展
    graphicsBase::Base().AddDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    // 创建Vulkan实例
    if (graphicsBase::Base().CreateInstance()) return false;

    // 创建Window Surface
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (result_t result =
            glfwCreateWindowSurface(graphicsBase::Base().Instance(), pWindow, nullptr, &surface)) {
        std::cout << std::format(
            "[ InitializeWindow ] ERROR\nFailed to create a window surface!\nError code: {}\n",
            int32_t(result)
        );

        glfwTerminate();
        return false;
    }
    graphicsBase::Base().Surface(surface);

    // 创建逻辑设备
    if (graphicsBase::Base().GetPhysicalDevices() ||
        graphicsBase::Base().DeterminePhysicalDevice(0, true, false) ||
        graphicsBase::Base().CreateDevice()) {
        return false;
    }
    // 创建交换链
    if (graphicsBase::Base().CreateSwapchain(limitFrameRate)) { return false; }

    return true;
}

inline void TerminateWindow() {
    graphicsBase::Base().WaitIdle();
    glfwTerminate();
}

inline void TitleFps() {
    static double time0 = glfwGetTime();
    static double time1;
    static double dt;
    static int dframe = -1;
    static std::stringstream info;
    time1 = glfwGetTime();
    dframe++;
    if ((dt = time1 - time0) >= 1) {
        info.precision(1);
        info << windowTitle << "    " << std::fixed << dframe / dt << " FPS";
        glfwSetWindowTitle(pWindow, info.str().c_str());
        info.str("");
        time0 = time1;
        dframe = 0;
    }
}
