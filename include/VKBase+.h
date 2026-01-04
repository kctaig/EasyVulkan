#pragma once

#include "VKBase.h"

struct graphicsPipelineCreateInfoPack {
    VkGraphicsPipelineCreateInfo createInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    /**
     * Vertex input
     * 顶点输入状态用于指定顶点数据的内容，以及如何输入顶点数据
     * VkVertexInputBindingDescription： 描述顶点缓冲区的绑定信息，包括绑定点、步幅和输入率
     * VkVertexInputAttributeDescription：描述顶点着色器的输入属性的信息，包括位置、格式和偏移量
     */
    VkPipelineVertexInputStateCreateInfo vertexInputStateCi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    std::vector<VkVertexInputBindingDescription> vertexInputBindings;
    std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;

    /**
     * Input Assembly
     * 主要用于指定输入的图元拓扑类型
     */
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCi = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};

    /**
     * Tessellation
     * 细分状态用于在进行细分的管线中，指定控制点的数量
     */
    VkPipelineTessellationStateCreateInfo tessellationStateCi = {VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};

    /**
     * Viewport
     * 视口状态用于指定视口和裁剪范围
     */
    VkPipelineViewportStateCreateInfo viewportStateCi = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    std::vector<VkViewport> viewports;
    std::vector<VkRect2D> scissors;
    uint32_t dynamicViewportCount = 1;
    uint32_t dynamicScissorCount = 1;

    /**
     * Rasterization
     * 光栅化状态指定在光栅化阶段及该阶段前的操作
     */
    VkPipelineRasterizationStateCreateInfo rasterizationStateCi = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};

    // Multisample
    VkPipelineMultisampleStateCreateInfo multisampleStateCi = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};

    // Depth & Stencil
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCi = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

    // Color blend
    VkPipelineColorBlendStateCreateInfo colorBlendStateCi = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;

    // Dynamic
    VkPipelineDynamicStateCreateInfo dynamicStateCi = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    std::vector<VkDynamicState> dynamicStates;

    //------------------------------------------

    graphicsPipelineCreateInfoPack() {
        SetCreateInfos();
        // 若非派生管线，createInfo.basePipelineIndex不得为，设置为-1
        createInfo.basePipelineIndex = -1;
    }

    // 移动构造器，所有指针都要重新赋值
    graphicsPipelineCreateInfoPack(const graphicsPipelineCreateInfoPack& other) noexcept {
        createInfo = other.createInfo;
        SetCreateInfos();

        vertexInputStateCi = other.vertexInputStateCi;
        inputAssemblyStateCi = other.inputAssemblyStateCi;
        tessellationStateCi = other.tessellationStateCi;
        viewportStateCi = other.viewportStateCi;
        rasterizationStateCi = other.rasterizationStateCi;
        multisampleStateCi = other.multisampleStateCi;
        depthStencilStateCi = other.depthStencilStateCi;
        colorBlendStateCi = other.colorBlendStateCi;
        dynamicStateCi = other.dynamicStateCi;

        shaderStages = other.shaderStages;
        vertexInputBindings = other.vertexInputBindings;
        vertexInputAttributes = other.vertexInputAttributes;
        viewports = other.viewports;
        scissors = other.scissors;
        colorBlendAttachmentStates = other.colorBlendAttachmentStates;
        dynamicStates = other.dynamicStates;
        UpdateAllArrayAddresses();
    }

    // Getter
    operator VkGraphicsPipelineCreateInfo&() { return createInfo; }

    // 将各个数组的地址更新到createInfo中，并更新各个数组的count
    void UpdateAllArrays() {
        createInfo.stageCount = shaderStages.size();
        vertexInputStateCi.vertexBindingDescriptionCount = vertexInputBindings.size();
        vertexInputStateCi.vertexAttributeDescriptionCount = vertexInputAttributes.size();
        viewportStateCi.viewportCount = viewports.size() ? static_cast<uint32_t>(viewports.size()) : dynamicViewportCount;
        viewportStateCi.scissorCount = scissors.size() ? static_cast<uint32_t>(scissors.size()) : dynamicScissorCount;
        colorBlendStateCi.attachmentCount = colorBlendAttachmentStates.size();
        dynamicStateCi.dynamicStateCount = dynamicStates.size();
        UpdateAllArrayAddresses();
    }

   private:
    // 将创建信息的地址赋给basePipelineIndex中的相应成员
    void SetCreateInfos() {
        createInfo.pVertexInputState = &vertexInputStateCi;
        createInfo.pInputAssemblyState = &inputAssemblyStateCi;
        createInfo.pTessellationState = &tessellationStateCi;
        createInfo.pViewportState = &viewportStateCi;
        createInfo.pRasterizationState = &rasterizationStateCi;
        createInfo.pMultisampleState = &multisampleStateCi;
        createInfo.pDepthStencilState = &depthStencilStateCi;
        createInfo.pColorBlendState = &colorBlendStateCi;
        createInfo.pDynamicState = &dynamicStateCi;
    }

    // 将所有数组的地址更新到createInfo中，但不改变各个数组的count
    void UpdateAllArrayAddresses() {
        createInfo.pStages = shaderStages.data();
        vertexInputStateCi.pVertexBindingDescriptions = vertexInputBindings.data();
        vertexInputStateCi.pVertexAttributeDescriptions = vertexInputAttributes.data();
        viewportStateCi.pViewports = viewports.data();
        viewportStateCi.pScissors = scissors.data();
        colorBlendStateCi.pAttachments = colorBlendAttachmentStates.data();
        dynamicStateCi.pDynamicStates = dynamicStates.data();
    }
};