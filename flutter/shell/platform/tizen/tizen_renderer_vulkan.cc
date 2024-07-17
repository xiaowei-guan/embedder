
// Copyright 2024 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/tizen/tizen_renderer_vulkan.h"
#include <vulkan/vulkan_wayland.h>
#include "flutter/shell/platform/tizen/logger.h"

namespace flutter {
TizenRendererVulkan::TizenRendererVulkan(TizenViewBase* view) {
  TizenRenderer::CreateSurface(view);
}

bool TizenRendererVulkan::CreateInstance() {
  uint32_t instance_extension_count = 0;
  bool has_surface_extension = false;

  if (vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count,
                                             NULL)) {
    FT_LOG(Error) << "Failed to enumerate instance extension count";
    return false;
  }

  if (instance_extension_count > 0) {
    std::vector<VkExtensionProperties> instance_extension_properties(
        instance_extension_count);

    if (vkEnumerateInstanceExtensionProperties(
            NULL, &instance_extension_count,
            instance_extension_properties.data())) {
      FT_LOG(Error) << "Failed to enumerate instance extension properties";
      return false;
    }
    for (const auto& ext : instance_extension_properties) {
      if (!strcmp(VK_KHR_SURFACE_EXTENSION_NAME, ext.extensionName)) {
        has_surface_extension = true;
        instance_extensions_.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
      } else if (!strcmp(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
                         ext.extensionName)) {
        has_surface_extension = true;
        instance_extensions_.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
      }
    }
  }

  if (!has_surface_extension) {
    FT_LOG(Error) << "vkEnumerateInstanceExtensionProperties failed to find "
                     "the extensions";
    return false;
  }

  // Create instance.
  VkApplicationInfo app_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = nullptr,
      .pApplicationName = "Tizen Embedder",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "Tizen Embedder",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0,
  };
  const VkInstanceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .pApplicationInfo = &app_info,
      .enabledExtensionCount =
          static_cast<uint32_t>(instance_extensions_.size()),
      .ppEnabledExtensionNames = instance_extensions_.data(),
  };

  if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS) {
    FT_LOG(Error) << "Create instance failed.";
    return false;
  }
  return true;
}

TizenRendererVulkan::~TizenRendererVulkan() {}
bool TizenRendererVulkan::CreateSurface(void* render_target,
                                        void* render_target_display,
                                        int32_t width,
                                        int32_t height) {
  return false;
}

void TizenRendererVulkan::DestroySurface() {}

void TizenRendererVulkan::ResizeSurface(int32_t width, int32_t height) {}
uint32_t TizenRendererVulkan::GetVersion() {
  return 0;
}

FlutterVulkanInstanceHandle TizenRendererVulkan::GetInstanceHandle() {
  return nullptr;
}

FlutterVulkanQueueHandle TizenRendererVulkan::GetQueueHandle() {
  return nullptr;
}

FlutterVulkanPhysicalDeviceHandle
TizenRendererVulkan::GetPhysicalDeviceHandle() {
  return nullptr;
}

FlutterVulkanDeviceHandle TizenRendererVulkan::GetDeviceHandle() {
  return nullptr;
}

uint32_t TizenRendererVulkan::GetQueueIndex() {
  return 0;
}

size_t TizenRendererVulkan::GetEnabledInstanceExtensionCount() {
  return 0;
}

const char** TizenRendererVulkan::GetEnabledInstanceExtensions() {
  return nullptr;
}

size_t TizenRendererVulkan::GetEnabledDeviceExtensionCount() {
  return 0;
}

const char** TizenRendererVulkan::GetEnabledDeviceExtensions() {
  return nullptr;
}

void* TizenRendererVulkan::GetInstanceProcAddress(
    FlutterVulkanInstanceHandle instance,
    const char* name) {
  return nullptr;
}

FlutterVulkanImage TizenRendererVulkan::GetNextImage(
    const FlutterFrameInfo* frameInfo) {
  return FlutterVulkanImage();
}

bool TizenRendererVulkan::Present(const FlutterVulkanImage* image) {
  return false;
}

}  // namespace flutter
