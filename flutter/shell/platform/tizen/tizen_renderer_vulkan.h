
// Copyright 2024 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_TIZEN_RENDERER_VULKAN_H_
#define EMBEDDER_TIZEN_RENDERER_VULKAN_H_

#include "flutter/shell/platform/tizen/tizen_renderer.h"
#include "flutter/shell/platform/tizen/tizen_view_base.h"

#include <vulkan/vulkan.h>

namespace flutter {

class TizenRendererVulkan : public TizenRenderer {
 public:
  explicit TizenRendererVulkan(TizenViewBase* view);
  virtual ~TizenRendererVulkan();

  bool CreateSurface(void* render_target,
                     void* render_target_display,
                     int32_t width,
                     int32_t height) override;

  void DestroySurface() override;

  void ResizeSurface(int32_t width, int32_t height) override;
  uint32_t GetVersion();
  FlutterVulkanInstanceHandle GetInstanceHandle();
  FlutterVulkanQueueHandle GetQueueHandle();
  FlutterVulkanPhysicalDeviceHandle GetPhysicalDeviceHandle();
  FlutterVulkanDeviceHandle GetDeviceHandle();
  uint32_t GetQueueIndex();
  size_t GetEnabledInstanceExtensionCount();
  const char** GetEnabledInstanceExtensions();
  size_t GetEnabledDeviceExtensionCount();
  const char** GetEnabledDeviceExtensions();
  void* GetInstanceProcAddress(FlutterVulkanInstanceHandle instance,
                               const char* name);
  FlutterVulkanImage GetNextImage(const FlutterFrameInfo* frameInfo);
  bool Present(const FlutterVulkanImage* image);

 private:
  static constexpr VkPresentModeKHR kPreferredPresentMode =
      VK_PRESENT_MODE_FIFO_KHR;
  bool CreateInstance();
  bool CreateLogicalDevice();
  void Cleanup();
  bool CheckValidationLayerSupport();
  bool GetRequiredExtensions(std::vector<const char*>& extensions);
  bool InitializeSwapchain();
  void PopulateDebugMessengerCreateInfo(
      VkDebugUtilsMessengerCreateInfoEXT& createInfo);
  void InitVulkan();
  bool PickPhysicalDevice();
  void SetupDebugMessenger();

  bool enable_validation_layers_ = true;

  VkDebugUtilsMessengerEXT debug_messenger_;
  VkDevice logical_device_;
  VkInstance instance_;
  VkPhysicalDevice physical_device_;
  VkQueue graphics_queue_;
  VkSurfaceKHR surface_;
  VkSurfaceFormatKHR surface_format_;
  VkSwapchainKHR swapchain_;
  VkCommandPool swapchain_command_pool_;
  std::vector<VkImage> swapchain_images_;
  std::vector<VkCommandBuffer> present_transition_buffers_;
  std::vector<const char*> enabled_device_extensions_;
  std::vector<const char*> enabled_instance_extensions_;
  uint32_t graphics_queue_family_index_;
  uint32_t width_;
  uint32_t height_;
};
}  // namespace flutter

#endif  // EMBEDDER_TIZEN_RENDERER_VULKAN_H_
