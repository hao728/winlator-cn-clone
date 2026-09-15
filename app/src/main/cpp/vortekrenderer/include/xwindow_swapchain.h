#ifndef VORTEK_XWINDOW_SWAPCHAIN_H
#define VORTEK_XWINDOW_SWAPCHAIN_H

#include <android/hardware_buffer.h>

#include "vortek.h"

typedef struct XWindowSwapchain_Image {
    VkImage image;
    VkDeviceMemory memory;
} XWindowSwapchain_Image;

typedef struct XWindowSwapchain {
    int windowId;
    XWindowSwapchain_Image* images;
    int imageCount;
    VkFormat imageFormat;
    VkExtent2D imageExtent;
    VkImageUsageFlags imageUsage;
    VkQueue queue;
    JMethods* jmethods;
    // --- win-fg 帧生成（native 集成）---
    // Why: win-fg 作为 Vulkan layer 时生成帧会被 host compositor 丢弃（仅~25%提升）。
    //      native 模式把帧生成编译进 compositor，在 present 路径直接插入中间帧。
    // What: 每个 swapchain 维护独立的 WinFgEngine（含 prev/curr 输入 ring）。
    // How: 删除以下字段及 xwindow_swapchain.c 中 winfg_* 调用即回退到无帧生成版本。
    VkPhysicalDevice phys;
    VkDevice device;
    uint32_t queueFamily;
    void* winfgEngine;          // WinFgEngine*（C++ 对象，C 语言用 void*）
    int winfgEnabled;           // 0=关闭, 1=开启（由 Java 层控制）
    int winfgMultiplier;        // 2/3/4
    int winfgModel;             // 3=optical flow, 4=bidirectional
    int winfgPreset;            // 0=质量, 1=平衡, 2=性能
    float winfgFlowScale;       // 0.25~1.0
    VkImage genImage;           // 生成帧临时 image（STORAGE usage）
    VkDeviceMemory genMem;
    VkImageView genView;
    VkCommandPool cmdPool;
    VkCommandBuffer cmdBuffer;
    VkFence fence;
} XWindowSwapchain;

extern void getWindowExtent(JMethods* jmethods, int windowId, VkExtent2D* extent);
extern int getSurfaceMinImageCount();
extern VkSurfaceFormatKHR* getSurfaceFormats(uint32_t* formatCount);

extern XWindowSwapchain* XWindowSwapchain_create(VkDevice device, uint32_t graphicsQueueIndex,
                                                 VkSwapchainCreateInfoKHR* swapchainInfo,
                                                 JMethods* jmethods, int windowId,
                                                 VkPhysicalDevice phys);
extern void XWindowSwapchain_destroy(VkDevice device, XWindowSwapchain* swapchain);
extern VkResult XWindowSwapchain_acquireNextImage(XWindowSwapchain* swapchain, uint64_t timeout, VkSemaphore signalSemaphore, VkFence fence, uint32_t* imageIndex);
extern void XWindowSwapchain_presentImage(XWindowSwapchain* swapchain);

// win-fg 运行时控制（由 JNI 层调用）
extern void XWindowSwapchain_setWinFgEnabled(XWindowSwapchain* swapchain, int enabled);
extern void XWindowSwapchain_setWinFgConfig(XWindowSwapchain* swapchain, int multiplier, int model, int preset, float flowScale);
extern int XWindowSwapchain_isWinFgActive(XWindowSwapchain* swapchain);

#endif
