#pragma once

#include "VKBase.h"
#include "VKFormat.h"

namespace vulkan {

class graphicsBasePlus {
    VkFormatProperties formatProperties[std::size(formatInfos_v1_0)] = {};
    commandPool commandPool_graphics{};
    commandPool commandPool_presentation{};
    commandPool commandPool_compute{};
    commandBuffer commandBuffer_transfer{};  // 从commandPool_graphics分配
    commandBuffer commandBuffer_presentation{};

    // 唯一实例
    static graphicsBasePlus singleton;

    // 私有防止外部创建
    graphicsBasePlus() {
        auto Initialize = [] {
            if (graphicsBase::Base().QueueFamilyIndex_Graphics() != VK_QUEUE_FAMILY_IGNORED) {
                singleton.commandPool_graphics.Create(
                    graphicsBase::Base().QueueFamilyIndex_Graphics(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
                );
                singleton.commandPool_graphics.AllocateBuffers(singleton.commandBuffer_transfer);
            }
            if (graphicsBase::Base().QueueFamilyIndex_Compute() != VK_QUEUE_FAMILY_IGNORED) {
                singleton.commandPool_compute.Create(
                    graphicsBase::Base().QueueFamilyIndex_Compute(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
                );
            }
            if (graphicsBase::Base().QueueFamilyIndex_Presentation() != VK_QUEUE_FAMILY_IGNORED &&
                graphicsBase::Base().QueueFamilyIndex_Presentation() !=
                    graphicsBase::Base().QueueFamilyIndex_Graphics() &&
                graphicsBase::Base().SwapchainCreateInfo().imageSharingMode == VK_SHARING_MODE_EXCLUSIVE) {
                singleton.commandPool_presentation.Create(
                    graphicsBase::Base().QueueFamilyIndex_Presentation(),
                    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
                );
                singleton.commandPool_presentation.AllocateBuffers(singleton.commandBuffer_presentation);
            }
            for (size_t i = 0; i < std::size(singleton.formatProperties); i++) {
                vkGetPhysicalDeviceFormatProperties(
                    graphicsBase::Base().PhysicalDevice(), VkFormat(i), &singleton.formatProperties[i]
                );
            }
        };

        auto CleanUp = [] {
            singleton.commandPool_graphics.~commandPool();
            singleton.commandPool_presentation.~commandPool();
            singleton.commandPool_compute.~commandPool();
        };

        graphicsBase::Plus(singleton);
        graphicsBase::Base().AddCallback_CreateDevice(Initialize);
        graphicsBase::Base().AddCallback_DestroyDevice(CleanUp);
    }
    graphicsBasePlus(graphicsBasePlus&&) = delete;
    ~graphicsBasePlus() = default;

  public:
    // Getter
    const commandPool& CommandPool_Graphics() const { return commandPool_graphics; }
    const commandPool& CommandPool_Compute() const { return commandPool_compute; }
    const commandBuffer& CommandBuffer_Transfer() const { return commandBuffer_transfer; }

    const VkFormatProperties& FormatProperties(VkFormat format) const {
#ifndef NDEBUG
        if (static_cast<uint32_t>(format) >= std::size(formatInfos_v1_0)) {
            outStream << std::format(
                "[ FormatProperties ] ERROR\nThis function only supports definite formats provided "
                "by VK_VERSION_1_0.\n"
            );
            abort();
        }
#endif
        return formatProperties[format];
    }

    // Const Function
    result_t ExecuteCommandBuffer_Graphics(VkCommandBuffer commandBuffer) const {
        fence fence;
        VkSubmitInfo submitInfo = {.commandBufferCount = 1, .pCommandBuffers = &commandBuffer};
        VkResult result = graphicsBase::Base().SubmitCommandBuffer_Graphics(submitInfo, fence);
        if (!result) { fence.Wait(); }
        return result;
    }

    // 提交命令缓冲区到呈现队列，进行图像所有权转移
    // result_t AcquireImageOwnership_Presentation(VkSemaphore semaphore_renderingIsOver,
    // VkSemaphore semaphore_ownershipIsTransfered,
    //                                             VkFence fence = VK_NULL_HANDLE) {
    //     if (VkResult result =
    //     commandBuffer_presentation.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)) {
    //         return result;
    //     }
    //     graphicsBase::Base().CmdTransferImageOwnership(commandBuffer_presentation);
    //     if (VkResult result = commandBuffer_presentation.End()) {
    //         return result;
    //     }
    //     return graphicsBase::Base().SubmitCommandBuffer_Presentation(commandBuffer_presentation,
    //     semaphore_renderingIsOver,
    //                                                                  semaphore_ownershipIsTransfered,
    //                                                                  fence);
    // }
};
inline graphicsBasePlus graphicsBasePlus::singleton;

constexpr formatInfo FormatInfo(VkFormat format) {
#ifndef NDEBUG
    if (static_cast<uint32_t>(format) >= std::size(formatInfos_v1_0)) {
        outStream << std::format(
            "[ FormatInfo ] ERROR\nThis function only supports definite formats provided by "
            "VK_VERSION_1_0.\n"
        );
        abort();
    }
#endif
    return formatInfos_v1_0[format];
}

constexpr VkFormat Corresponding16BitFloatFormat(VkFormat format_32BitFloat) {
    switch (format_32BitFloat) {
        case VK_FORMAT_R32_SFLOAT:
            return VK_FORMAT_R16_SFLOAT;
        case VK_FORMAT_R32G32_SFLOAT:
            return VK_FORMAT_R16G16_SFLOAT;
        case VK_FORMAT_R32G32B32_SFLOAT:
            return VK_FORMAT_R16G16B16_SFLOAT;
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
    }
    return format_32BitFloat;
}

inline const VkFormatProperties& FormatProperties(VkFormat format) {
    return graphicsBase::Plus().FormatProperties(format);
}

class stagingBuffer {
    static inline class {
        stagingBuffer* pointer = Create();
        stagingBuffer* Create() {
            static stagingBuffer stagingBuffer;
            pointer = &stagingBuffer;
            graphicsBase::Base().AddCallback_DestroyDevice([] { stagingBuffer.~stagingBuffer(); });
            return &stagingBuffer;
        }

      public:
        stagingBuffer& Get() const { return *pointer; }
    } stagingBuffer_mainThread;

  protected:
    bufferMemory bufferMemory;
    VkDeviceSize memoryUsage = 0;  // 每次映射的内存大小
    image aliasedImage;

  public:
    stagingBuffer() = default;
    stagingBuffer(VkDeviceSize size) { Expand(size); }
    // Getter
    operator VkBuffer() const { return bufferMemory.Buffer(); }
    const VkBuffer* Address() const { return bufferMemory.AddressOfBuffer(); }
    VkDeviceSize AllocateSize() const { return bufferMemory.AllocationSize(); }
    VkImage AliasedImage() const { return aliasedImage; }
    // Const function
    // 从缓冲区中取回数据
    void RetrieveData(void* pData_src, VkDeviceSize size) const { bufferMemory.RetrieveData(pData_src, size); }
    // Non-const function
    // 所分配设备内存大小不够时重新分配
    void Expand(VkDeviceSize size) {
        if (size <= bufferMemory.AllocationSize()) { return; }
        Release();
        VkBufferCreateInfo bufferCreateInfo = {
            .size = size, .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        };
        bufferMemory.Create(bufferCreateInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }
    // 手动释放所有内存并销毁设备内存和缓冲区的handle
    void Release() { bufferMemory.~bufferMemory(); }
    void* MapMemory(VkDeviceSize size) {
        Expand(size);
        void* pData_dst = nullptr;
        bufferMemory.MapMemory(pData_dst, size);
        memoryUsage = size;
        return pData_dst;
    }
    void UnmapMemory() {
        bufferMemory.UnmapMemory(memoryUsage);
        memoryUsage = 0;
    }
    // 向缓冲区写入数据
    void BufferData(const void* pData_src, VkDeviceSize size) {
        Expand(size);
        bufferMemory.BufferData(pData_src, size);
    }
    // 创建线性布局的混叠2d图像
    [[nodiscard]] VkImage AliasedImage2D(VkFormat format, VkExtent2D extent) {
        if (!(FormatProperties(format).linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
            return VK_NULL_HANDLE;
        }
        VkDeviceSize imageDataSize =
            static_cast<VkDeviceSize>(FormatInfo(format).sizePerPixel) * extent.width * extent.height;
        if (imageDataSize > bufferMemory.AllocationSize()) { return VK_NULL_HANDLE; }
        VkImageFormatProperties imageFormatProperties = {};
        vkGetPhysicalDeviceImageFormatProperties(
            graphicsBase::Base().PhysicalDevice(), format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_LINEAR,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0, &imageFormatProperties
        );
        if (extent.width > imageFormatProperties.maxExtent.width ||
            extent.height > imageFormatProperties.maxExtent.height ||
            imageDataSize > imageFormatProperties.maxResourceSize) {
            return VK_NULL_HANDLE;
        }
        VkImageCreateInfo imageCreateInfo = {
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = {extent.width, extent.height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_LINEAR,
            .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
        };
        aliasedImage.~image();
        aliasedImage.Create(imageCreateInfo);
        VkImageSubresource subResource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
        VkSubresourceLayout subresourceLayout = {};
        vkGetImageSubresourceLayout(graphicsBase::Base().Device(), aliasedImage, &subResource, &subresourceLayout);
        if (subresourceLayout.size != imageDataSize) { return VK_NULL_HANDLE; }
        aliasedImage.BindMemory(bufferMemory.Memory());
        return aliasedImage;
    }
    // Static function
    static VkBuffer Buffer_MianThread() { return stagingBuffer_mainThread.Get(); }
    static void Expand_MainThread(VkDeviceSize size) { stagingBuffer_mainThread.Get().Expand(size); }
    static void Release_MainThread() { stagingBuffer_mainThread.Get().Release(); }
    static void* MapMemory_MainThread(VkDeviceSize size) { return stagingBuffer_mainThread.Get().MapMemory(size); }
    static void UnmapMemory_MainThread() { stagingBuffer_mainThread.Get().UnmapMemory(); }
    static void BufferData_MainThread(const void* pData_src, VkDeviceSize size) {
        stagingBuffer_mainThread.Get().BufferData(pData_src, size);
    }
    static void RetrieveData_MainThread(void* pData_src, VkDeviceSize size) {
        stagingBuffer_mainThread.Get().RetrieveData(pData_src, size);
    }
    [[nodiscard]] static VkImage AliasedImage2D_MainThread(VkFormat format, VkExtent2D extent) {
        return stagingBuffer_mainThread.Get().AliasedImage2D(format, extent);
    }
};

// 将具有VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT属性的缓冲区进行封装的类
class deviceLocalBuffer {
  protected:
    bufferMemory bufferMemory;

  public:
    deviceLocalBuffer() = default;
    deviceLocalBuffer(VkDeviceSize size, VkBufferUsageFlags desiredUsages_Without_transfer_dst) {
        Create(size, desiredUsages_Without_transfer_dst);
    }
    // Getter
    explicit operator VkBuffer() const { return bufferMemory.Buffer(); }
    const VkBuffer* Address() const { return bufferMemory.AddressOfBuffer(); }
    VkDeviceSize AllocateSize() const { return bufferMemory.AllocationSize(); }
    // Const function

    // 适用于更新连续的数据块
    void TransferData(const void* pData_src, VkDeviceSize size, VkDeviceSize offset = 0) const {
        if (bufferMemory.MemoryProperties() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            bufferMemory.BufferData(pData_src, size, offset);
            return;
        }
        /*
         * 具有VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT，但不具有VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT内存属性
         * 需要借助暂存缓冲区进行数据传输
         * 1. 将数据写入暂存缓冲区
         * 2. 通过命令缓冲区将暂存缓冲区的数据复制到目标缓冲区
         * 3. 提交命令缓冲区并等待执行完成
         */
        stagingBuffer::BufferData_MainThread(pData_src, size);
        auto& commandBuffer = graphicsBase::Plus().CommandBuffer_Transfer();
        commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VkBufferCopy region = {0, offset, size};
        vkCmdCopyBuffer(commandBuffer, stagingBuffer::Buffer_MianThread(), bufferMemory.Buffer(), 1, &region);
        commandBuffer.End();
        graphicsBase::Plus().ExecuteCommandBuffer_Graphics(commandBuffer);
    }

    // 适用于更新不连续的多块数据，stride是每组数据间的步长
    void TransferData(
        const void* pData_src, uint32_t elementCount, VkDeviceSize elementsSize, VkDeviceSize stride_src,
        VkDeviceSize stride_dst, VkDeviceSize offset = 0
    ) const {
        if (bufferMemory.MemoryProperties() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            void* pData_dst = nullptr;
            bufferMemory.MapMemory(pData_dst, elementCount * stride_dst, offset);
            for (size_t i = 0; i < elementCount; i++) {
                memcpy(
                    stride_dst * i + static_cast<uint8_t*>(pData_dst),
                    stride_src * i + static_cast<const uint8_t*>(pData_src), static_cast<size_t>(elementCount)
                );
            }
            bufferMemory.UnmapMemory(elementCount * stride_dst, offset);
            return;
        }
        stagingBuffer::BufferData_MainThread(pData_src, stride_src * elementCount);
        auto& commandBuffer = graphicsBase::Plus().CommandBuffer_Transfer();
        commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        auto regions = std::make_unique<VkBufferCopy[]>(elementCount);
        for (size_t i = 0; i < elementCount; i++) {
            regions[i] = {stride_src * i, stride_dst * i + offset, elementsSize};
        }
        vkCmdCopyBuffer(
            commandBuffer, stagingBuffer::Buffer_MianThread(), bufferMemory.Buffer(), elementCount, regions.get()
        );
        commandBuffer.End();
        graphicsBase::Plus().ExecuteCommandBuffer_Graphics(commandBuffer);
    }

    // 适用于从缓冲区开头更新连续的数据块，数据大小自动判断
    void TransferData(const auto& data_src) const { TransferData(&data_src, sizeof data_src); }

    // vkCmdUpdateBuffer 将更新数据直接记录在commandBuffer中，它会影响commandBuffer大小
    void CmdUpdateBuffer(
        VkCommandBuffer commandBuffer, const void* pData_src, VkDeviceSize size_Limited_to_65536,
        VkDeviceSize offset = 0
    ) const {
        vkCmdUpdateBuffer(commandBuffer, bufferMemory.Buffer(), offset, size_Limited_to_65536, pData_src);
    }
    // 适用于从缓冲区开头更新连续的数据块，数据大小自动判断
    void CmdUpdateBuffer(VkCommandBuffer commandBuffer, const auto& data_src) const {
        vkCmdUpdateBuffer(commandBuffer, bufferMemory.Buffer(), 0, sizeof data_src, &data_src);
    };

    // Non-const function
    void Create(VkDeviceSize size, VkBufferUsageFlags desiredUsages_Without_transfer_dst) {
        VkBufferCreateInfo bufferCreateInfo = {
            .size = size,
            .usage = desiredUsages_Without_transfer_dst | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        };
        // 若左边为true，右边不会执行
        // 创建buffer，为buffer分配内存，绑定内存
        false || bufferMemory.CreateBuffer(bufferCreateInfo) ||
            // 优先尝试分配同时具有device local和host visible属性的内存,
            bufferMemory.AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
                bufferMemory.AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ||
            bufferMemory.BindMemory();
    }
    void Recreate(VkDeviceSize size, VkBufferUsageFlags desiredUsages_Without_transfer_dst) {
        // deviceLocalBuffer重建时需要确保GPU不再使用该缓冲区
        graphicsBase::Base().WaitIdle();
        bufferMemory.~bufferMemory();
        Create(size, desiredUsages_Without_transfer_dst);
    }
};

// 为顶点缓冲区创建的封装类
class vertexBuffer : public deviceLocalBuffer {
  public:
    vertexBuffer() = default;
    // 在创建缓冲区时默认指定了VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    vertexBuffer(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0)
        : deviceLocalBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | otherUsages) {}
    // Non-const function
    void Create(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0) {
        deviceLocalBuffer::Create(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | otherUsages);
    }
    void Recreate(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0) {
        deviceLocalBuffer::Recreate(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | otherUsages);
    }
};

class indexBuffer : public deviceLocalBuffer {
  public:
    indexBuffer() = default;
    indexBuffer(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0)
        : deviceLocalBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | otherUsages) {}
    void Create(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0) {
        deviceLocalBuffer::Create(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | otherUsages);
    }
    void Recreate(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0) {
        deviceLocalBuffer::Recreate(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | otherUsages);
    }
};

class uniformBuffer : public deviceLocalBuffer {
  public:
    uniformBuffer() = default;
    uniformBuffer(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0)
        : deviceLocalBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | otherUsages) {}
    void Create(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0) {
        deviceLocalBuffer::Create(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | otherUsages);
    }
    void Recreate(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0) {
        deviceLocalBuffer::Recreate(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | otherUsages);
    }
    // Static function
    static VkDeviceSize CalculateAlignedSize(VkDeviceSize dataSize) {
        const VkDeviceSize& alignment =
            graphicsBase::Base().PhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;
        return dataSize + alignment - 1 & ~(alignment - 1);
    }
};

class storageBuffer : public deviceLocalBuffer {
  public:
    storageBuffer() = default;
    storageBuffer(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0)
        : deviceLocalBuffer(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {}
    // Non-const Function
    void Create(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0) {
        deviceLocalBuffer::Create(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | otherUsages);
    }
    void Recreate(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0) {
        deviceLocalBuffer::Recreate(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | otherUsages);
    }
    // Static Function
    static VkDeviceSize CalculateAlignedSize(VkDeviceSize dataSize) {
        const VkDeviceSize& alignment =
            graphicsBase::Base().PhysicalDeviceProperties().limits.minStorageBufferOffsetAlignment;
        return dataSize + alignment - 1 & ~(alignment - 1);
    }
};

struct graphicsPipelineCreateInfoPack {
    VkGraphicsPipelineCreateInfo createInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    /**
     * Vertex input
     * 顶点输入状态用于指定顶点数据的内容，以及如何输入顶点数据
     * VkVertexInputBindingDescription： 描述顶点缓冲区的绑定信息，包括绑定点、步幅和输入率
     * VkVertexInputAttributeDescription：描述顶点着色器的输入属性的信息，包括位置、格式和偏移量
     */
    VkPipelineVertexInputStateCreateInfo vertexInputStateCi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };
    std::vector<VkVertexInputBindingDescription> vertexInputBindings;
    std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;

    /**
     * Input Assembly
     * 主要用于指定输入的图元拓扑类型
     */
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCi = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
    };

    /**
     * Tessellation
     * 细分状态用于在进行细分的管线中，指定控制点的数量
     */
    VkPipelineTessellationStateCreateInfo tessellationStateCi = {
        VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO
    };

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
    VkPipelineRasterizationStateCreateInfo rasterizationStateCi = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO
    };

    // Multisample
    VkPipelineMultisampleStateCreateInfo multisampleStateCi = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO
    };

    // Depth & Stencil
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCi = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
    };

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
        viewportStateCi.viewportCount =
            viewports.size() ? static_cast<uint32_t>(viewports.size()) : dynamicViewportCount;
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
}  // namespace vulkan