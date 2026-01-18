#pragma once

#include "VKBase+.h"

using namespace vulkan;
const VkExtent2D& windowSize = graphicsBase::Base().SwapchainCreateInfo().imageExtent;

namespace easyVulkan {

using namespace vulkan;

struct renderPassWithFramebuffers {
    renderPass renderPass;
    std::vector<framebuffer> framebuffers;
};

// 创建一个直接渲染到交换链图像，且不做深度测试等任何测试的渲染通道和对应的帧缓冲
const auto& CreateRpwf_Screen() {
    static renderPassWithFramebuffers rpwf;

    // 描述图像附件，这里描述的是交换链图像
    VkAttachmentDescription attachmentDescription = {
        .format = graphicsBase::Base().SwapchainCreateInfo().imageFormat,  // 图像附件的格式
        .samples = VK_SAMPLE_COUNT_1_BIT,                                  // 每个像素的采样点数量
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,       // 读取图像附件时，对颜色和深度进行的操作
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,     // 存储颜色和深度值到图像附件时的操作
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,  // 读取图像附件时的内存布局
        .finalLayout =
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR  // 存储渲染结果到图像附件时，需转换至的内存布局
    };
    // 子通道描述：只有一个子通道，该子通道只使用一个颜色附件
    VkAttachmentReference attachmentReference = {
        0,  // 附件对应VkRenderPassCreateInfo::pAttachments中元素的索引
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL  // 该子通道内使用该附件时的内存布局
    };
    VkSubpassDescription subpassDescription = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,  // 该通道对应的管线类型
        .colorAttachmentCount = 1,                             // 该子通道中所使用颜色附件的数量
        .pColorAttachments = &attachmentReference              // 该子通道所使用的颜色附件数组
    };

    // 子通道依赖描述：外部操作与子通道的依赖关系
    VkSubpassDependency subpassDependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,                              // 源子通道
        .dstSubpass = 0,                                                // 目标子通道
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // 源管线阶段
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // 目标管线阶段
        .srcAccessMask = 0,                                             // 源操作
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // 目标操作
        .dependencyFlags =
            VK_DEPENDENCY_BY_REGION_BIT  // 自Vulkan1.0以来可以指定VK_DEPENDENCY_BY_REGION_BIT
    };

    // 渲染通道的创建信息
    VkRenderPassCreateInfo renderPassCreateInfo = {
        .attachmentCount = 1,                    // 图像附件的数量
        .pAttachments = &attachmentDescription,  // 描述图像附件数组
        .subpassCount = 1,                       // 子通道数量
        .pSubpasses = &subpassDescription,       // 描述子通道的数组
        .dependencyCount = 1,                    // 子通道依赖的数量
        .pDependencies = &subpassDependency      // 用于描述子通道依赖的数组
    };

    // 创建渲染通道
    rpwf.renderPass.Create(renderPassCreateInfo);

    // 创建帧缓冲
    auto CreateFramebuffers = [] {
        rpwf.framebuffers.resize(graphicsBase::Base().SwapchainImageCount());
        VkFramebufferCreateInfo framebufferCreateInfo = {
            .renderPass = rpwf.renderPass,  // 关联的渲染通道
            .attachmentCount = 1,           // 图像附件的数量
            .width = windowSize.width,      // 帧缓冲宽度
            .height = windowSize.height,    // 帧缓冲高度
            .layers = 1                     // 帧缓冲的图层数
        };
        for (size_t i = 0; i < graphicsBase::Base().SwapchainImageCount(); i++) {
            VkImageView attachment = graphicsBase::Base().SwapchainImageView(i);
            framebufferCreateInfo.pAttachments = &attachment;  // 指定图像附件
            rpwf.framebuffers[i].Create(framebufferCreateInfo);
        }
    };
    auto DestroyFramebuffers = [] { rpwf.framebuffers.clear(); };
    CreateFramebuffers();
    ExecuteOnce(rpwf);
    graphicsBase::Base().AddCallback_CreateSwapchain(CreateFramebuffers);
    graphicsBase::Base().AddCallback_DestroySwapchain(DestroyFramebuffers);

    return rpwf;
}

}  // namespace easyVulkan