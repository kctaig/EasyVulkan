#include "EasyVulkan.hpp"
#include "GlfwGeneral.hpp"

using namespace vulkan;

int main() {
    if (!InitializeWindow({1280, 720})) return -1;

    const auto& [renderPass, framebuffers] = easyVulkan::CreateRpwf_Screen();

    fence fence;  // 以非置位状态创建fence
    semaphore semaphore_imageIsAvailable;
    semaphore semaphore_renderingIsOver;

    commandBuffer commandBuffer;
    commandPool commandPool(graphicsBase::Base().QueueFamilyIndex_Graphics(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    commandPool.AllocateBuffers(commandBuffer);

    VkClearValue clearColor = {.color = {1.f, 0.f, 0.f, 1.f}};

    while (!glfwWindowShouldClose(pWindow)) {
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED)) glfwWaitEvents();

        // 获取交换链图像索引
        graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);
        auto i = graphicsBase::Base().CurrentImageIndex();

        commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        // 开始渲染通道
        renderPass.CmdBegin(commandBuffer, framebuffers[i], {{}, windowSize}, clearColor);
        // 结束渲染通道
        renderPass.CmdEnd(commandBuffer);
        commandBuffer.End();

        graphicsBase::Base().SubmitCommandBuffer_Graphics(commandBuffer, semaphore_imageIsAvailable, semaphore_renderingIsOver, fence);
        graphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        glfwPollEvents();
        TitleFps();

        fence.WaitAndReset();
    }
    TerminateWindow();
    return 0;
}
