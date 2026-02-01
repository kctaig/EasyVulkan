#include "EasyVulkan.hpp"
#include "GlfwGeneral.hpp"

using namespace vulkan;

struct vertex {
    glm::vec2 position;
    glm::vec4 color;
};

pipelineLayout pipelineLayout_triangle;
pipeline pipeline_triangle;
descriptorSetLayout descriptorSetLayout_triangle;

/**
 * 定义一个静态变量存储easyVulkan::CreateRpwf_Screen()返回值，确保只创建一次
 * 会有析构顺序问题，renderpass和framebuffer的析构会在main函数之后，此时device已经被销毁
 */
const auto& RenderPassAndFramebuffers() {
    static const auto& rpwf = easyVulkan::CreateRpwf_Screen();
    return rpwf;
}

void CreateLayout() {
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding_trianglePosition = {
        .binding = 0,                                         // 描述符被绑定到0号binding
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  // 类型为uniform缓冲区
        .descriptorCount = 1,                                 // 个数是1个
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT              // 在顶点着色器阶段读取uniform缓冲区
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo_triangle = {
        .bindingCount = 1, .pBindings = &descriptorSetLayoutBinding_trianglePosition
    };
    descriptorSetLayout_triangle.Create(descriptorSetLayoutCreateInfo_triangle);

    // VkPushConstantRange pushConstantRange = {
    //     VK_SHADER_STAGE_VERTEX_BIT,
    //     0,
    //     24,
    // };

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{// .pushConstantRangeCount = 1,
                                                        // .pPushConstantRanges = &pushConstantRange,
                                                        .setLayoutCount = 1,
                                                        .pSetLayouts = descriptorSetLayout_triangle.Address()
    };
    pipelineLayout_triangle.Create(pipelineLayoutCreateInfo);
}

void CreatePipeline() {
    static shaderModule vert("../shader/UniformBuffer.vert.spv");
    static shaderModule frag("../shader/VertexBuffer.frag.spv");
    static VkPipelineShaderStageCreateInfo shaderStageCreateInfos_triangles[2] = {
        vert.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
    };
    auto Create = [] {
        graphicsPipelineCreateInfoPack pipelineCiPack;
        pipelineCiPack.createInfo.layout = pipelineLayout_triangle;
        pipelineCiPack.createInfo.renderPass = RenderPassAndFramebuffers().renderPass;
        pipelineCiPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // 数据来自0号顶点缓冲区，逐顶点输入
        pipelineCiPack.vertexInputBindings.emplace_back(0, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX);
        /*
         * location=0,对应 “layout(location = 0) in”
         * binding=0,表还数据来自0号顶点缓冲区
         * vec3对应VK_FORMAT_R32G32B32A32_SFLOAT，用offsetof计算color在vertex中的起始位置
         */
        pipelineCiPack.vertexInputAttributes.emplace_back(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(vertex, position));
        pipelineCiPack.vertexInputAttributes.emplace_back(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, color));
        pipelineCiPack.viewports.emplace_back(
            0.f, 0.f, static_cast<float>(windowSize.width), static_cast<float>(windowSize.height), 0.f, 1.f
        );
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

    VkClearValue clearColor = {.color = {1.f, 0.f, 0.f, 1.f}};

    glm::vec2 uniform_positions[] = {
        {.0f, .0f}, {}, {-.5f, .0f}, {}, {.5f, .0f}, {},
    };
    uniformBuffer uniformBuffer(sizeof uniform_positions);
    uniformBuffer.TransferData(uniform_positions);

    vertex vertices[] = {
        {{.0f, -.5f}, {1, 0, 0, 1}},
        {{-.5f, .5f}, {0, 1, 0, 1}},
        {{.5f, .5f}, {0, 0, 1, 1}},
    };
    vertexBuffer vertexBuffer(sizeof vertices);
    vertexBuffer.TransferData(vertices);

    // 创建描述符集
    VkDescriptorPoolSize descriptorPoolSizes[] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};
    descriptorPool descriptorPool(1, arrayRef<const VkDescriptorPoolSize>(descriptorPoolSizes));
    // 分配描述符集
    descriptorSet descriptorSet_trianglePosition;
    descriptorPool.AllocateSets(
        arrayRef(descriptorSet_trianglePosition), arrayRef<const descriptorSetLayout>(descriptorSetLayout_triangle)
    );
    // 将uniform缓冲区信息写入描述符集
    VkDescriptorBufferInfo bufferInfo = {
        .buffer = static_cast<VkBuffer>(uniformBuffer),
        .offset = 0,
        .range = sizeof uniform_positions,
    };
    descriptorSet_trianglePosition.Write(
        arrayRef<const VkDescriptorBufferInfo>(bufferInfo), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    );

    /**
     * semaphore_imageIsAvailable 是 per-frame 的：因为每次 acquire 都是新请求。
     * 表示：通知 GPU 交换链中的某一张图像已经准备好，可以开始渲染了
     * semaphore_renderingIsOver 是 per-image 的：确保每个图像有自己的“渲染完成”信号。
     * 表示：通知 GPU 该交换链图像的渲染已经完成，可以进行展示
     * 因此，semaphore_imageIsAvailable 和 semaphore_renderingIsOver 不能共用一个索引
     */
    struct PerFrame {
        fence frameFence{VK_FENCE_CREATE_SIGNALED_BIT};  // 确保每个帧在第一次提交时不会被阻塞
        semaphore semaphore_imageIsAvailable;
        commandBuffer commandBuffer;
    };
    std::array<PerFrame, MAX_FRAMES_IN_FLIGHT> perFrame;
    std::vector<semaphore> semaphore_renderingIsOvers(graphicsBase::Base().SwapchainImageCount());

    commandPool commandPool(
        graphicsBase::Base().QueueFamilyIndex_Graphics(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    );
    for (auto& frame : perFrame) { commandPool.AllocateBuffers(arrayRef(frame.commandBuffer)); }
    uint32_t currentFrame = 0;

    // render loop
    while (!glfwWindowShouldClose(pWindow)) {
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED)) glfwWaitEvents();

        const auto& [frameFence, semaphore_imageIsAvailable, commandBuffer] = perFrame[currentFrame];

        // 当前帧的渲染命令不会在前一帧执行完之前开始，因此等待栅栏
        frameFence.WaitAndReset();

        // 获取下一帧要渲染的交换链图像索引
        graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);
        auto imageIndex = graphicsBase::Base().CurrentImageIndex();
        auto& semaphore_renderingIsOver = semaphore_renderingIsOvers[imageIndex];

        commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        // 开始渲染通道
        renderPass.CmdBegin(
            commandBuffer, framebuffers[imageIndex], {{}, windowSize}, arrayRef<const VkClearValue>(clearColor)
        );

        // 绑定顶点缓冲区
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffer.Address(), &offset);

        // 绑定图形管线并绘制三角形
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_triangle);

        // 推送常量数据到顶点着色器
        // vkCmdPushConstants(
        //     commandBuffer, pipelineLayout_triangle, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof pushConstants,
        //     pushConstants
        // );

        // 绑定描述符集
        vkCmdBindDescriptorSets(
            commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_triangle, 0, 1,
            descriptorSet_trianglePosition.Address(), 0, nullptr
        );

        vkCmdDraw(commandBuffer, 3, 3, 0, 0);

        // 结束渲染通道
        renderPass.CmdEnd(commandBuffer);
        commandBuffer.End();

        // 提交命令缓冲区到队列，GPU开始执行渲染命令
        graphicsBase::Base().SubmitCommandBuffer_Graphics(
            commandBuffer, semaphore_imageIsAvailable, semaphore_renderingIsOver, frameFence
        );
        // GPU等待渲染完成信号量，然后展示图像
        graphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        // 更新下一帧索引
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

        glfwPollEvents();
        TitleFps();
    }
    TerminateWindow();
    return 0;
}
