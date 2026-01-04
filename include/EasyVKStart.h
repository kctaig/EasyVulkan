#pragma once

#include <chrono>
#include <concepts>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <numbers>
#include <numeric>
#include <span>
#include <sstream>
#include <stack>
#include <unordered_map>
#include <vector>
#include <array>

// GLM
#define GLM_FORCE_DEPTH_ZEWRO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// stb_image
#include <stb_image.h>

// Vulkan
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#endif

#include <vulkan/vulkan.h>

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

template <typename T>
class arrayRef {
    T* const pArray = nullptr;
    size_t count = 0;

   public:
    // 从空参数构造，count为0
    arrayRef() = default;
    // 从单个对象构造，count为1
    arrayRef(T& data) : pArray(&data), count(1) {}
    // 从底层内存连续的标准库容器构造
    template <typename R>
        requires std::ranges::contiguous_range<R> &&    // 连续内存范围
                     std::ranges::sized_range<R> &&     // 知道大小
                     std::ranges::borrowed_range<R> &&  // 可借用（不拥有数据）
                     std::convertible_to<std::ranges::range_reference_t<R>,
                                         T>  // 元素类型可转换为T
    arrayRef(R&& range) : pArray(std::ranges::data(range)), count(std::ranges::size(range)) {}
    // 从指针和计数构造
    arrayRef(T* pData, size_t elementCount) : pArray(pData), count(elementCount) {}
    // 若T带const修饰，兼容从对应的无const修饰版本的arrayRef构造
    arrayRef(const arrayRef<std::remove_const_t<T>>& other) : pArray(other.Ponter()), count(other.Count()) {}

    // Gettter
    T* Pointer() const { return pArray; }
    size_t Count() const { return count; }

    // Const Function
    T& operator[](size_t index) const { return pArray[index]; }
    T* begin() const { return pArray; }
    T* end() const { return pArray + count; }

    // Non-cosnt Function
    // 禁止复制/移动赋值(arrayRef旨在模拟“对数组的引用”，用处归根结底只是传参，故使其同C++引用的底层地址一样，防止初始化后被修改)
    arrayRef& operator=(const arrayRef&) = delete;
};

// 只执行一次的宏
#define ExecuteOnce(...)                  \
    {                                     \
        static bool executed = false;     \
        if (executed) return __VA_ARGS__; \
        executed = true;                  \
    }
