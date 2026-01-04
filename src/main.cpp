#include "EasyVulkan.hpp"
#include "GlfwGeneral.hpp"

using namespace vulkan;

pipelineLayout pipelineLayout_triangle;
pipeline pipeline_triangle;

// 定义一个静态变量存储easyVulkan::CreateRpwf_Screen()返回值，确保只创建一次
const auto& RenderPassAndFramebuffers() {
    static const auto& rpwf = easyVulkan::CreateRpwf_Screen();
    return rpwf;
}

void CreateLayout() {
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayout_triangle.Create(pipelineLayoutCreateInfo);
}

void CreatePipeline() {
    static shaderModule vert("../shader/FirstTriangle.vert.spv");
    static shaderModule frag("../shader/FirstTriangle.frag.spv");
    static VkPipelineShaderStageCreateInfo shaderStageCreateInfos_triangles[2] = {vert.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                                                  frag.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};
    auto Create = [] {
        graphicsPipelineCreateInfoPack pipelineCiPack;
        pipelineCiPack.createInfo.layout = pipelineLayout_triangle;
        pipelineCiPack.createInfo.renderPass = RenderPassAndFramebuffers().renderPass;
        pipelineCiPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipelineCiPack.viewports.emplace_back(0.f, 0.f, float(windowSize.width), float(windowSize.height), 0.f, 1.f);
        pipelineCiPack.scissors.emplace_back(VkOffset2D{}, windowSize);
        pipelineCiPack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        pipelineCiPack.colorBlendAttachmentStates.push_back({.colorWriteMask = 0b1111});
        pipelineCiPack.UpdateAllArrays();
        pipelineCiPack.createInfo.stageCount = 2;
        pipelineCiPack.createInfo.pStages = shaderStageCreateInfos_triangles;
        pipeline_triangle.Create(pipelineCiPack);
    };
    auto Destroy = [] { pipeline_triangle.~pipeline(); };
    graphicsBase::Base().AddCallback_CreateSwapchain(Create);
    graphicsBase::Base().AddCallback_DestroySwapchain(Destroy);

    Create();
}

int main() {
    if (!InitializeWindow({1280, 720})) return -1;

    const auto& [renderPass, framebuffers] = easyVulkan::CreateRpwf_Screen();
    CreateLayout();
    CreatePipeline();

    struct perFrameObjects_t {
        fence fence = {VK_FENCE_CREATE_SIGNALED_BIT};
        semaphore semaphore_imageIsAvailable;
        semaphore semaphore_renderingIsOver;
        commandBuffer commandBuffer;
    };
    std::vector<perFrameObjects_t> perFrameObjects(graphicsBase::Base().SwapchainImageCount());
    commandPool commandPool(graphicsBase::Base().QueueFamilyIndex_Graphics(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    for (auto& i : perFrameObjects) {
        commandPool.AllocateBuffers(i.commandBuffer);
    }
    uint32_t currentFrame = 0;

    VkClearValue clearColor = {.color = {1.f, 0.f, 0.f, 1.f}};

    while (!glfwWindowShouldClose(pWindow)) {
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED)) glfwWaitEvents();

        const auto& [fence, semaphore_imageIsAvailable, semaphore_renderingIsOver, commandBuffer] = perFrameObjects[currentFrame];

        fence.WaitAndReset();
        graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);

        // 获取交换链图像索引
        auto imageIndex = graphicsBase::Base().CurrentImageIndex();

        commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        // 开始渲染通道
        renderPass.CmdBegin(commandBuffer, framebuffers[imageIndex], {{}, windowSize}, clearColor);
        
        // 绑定图形管线并绘制三角形
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_triangle);
        // 绘制三角形：3个顶点，1个实例，起始顶点索引0，起始实例索引0
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        // 结束渲染通道
        renderPass.CmdEnd(commandBuffer);
        commandBuffer.End();

        graphicsBase::Base().SubmitCommandBuffer_Graphics(commandBuffer, semaphore_imageIsAvailable, semaphore_renderingIsOver, fence);
        graphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        // Update current frame index
        currentFrame = (currentFrame + 1) % graphicsBase::Base().SwapchainImageCount();

        glfwPollEvents();
        TitleFps();
    }
    TerminateWindow();
    return 0;
}
