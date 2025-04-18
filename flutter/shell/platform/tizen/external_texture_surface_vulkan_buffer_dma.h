// Copyright 2024 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_TIZEN_EXTERNAL_TEXTURE_SURFACE_VULKAN_BUFFER_DMA_H_
#define FLUTTER_SHELL_PLATFORM_TIZEN_EXTERNAL_TEXTURE_SURFACE_VULKAN_BUFFER_DMA_H_

#include <vulkan/vulkan.h>
#include "flutter/shell/platform/tizen/external_texture_surface_vulkan_buffer.h"
#include "flutter/shell/platform/tizen/tizen_renderer_vulkan.h"

#include <tbm_surface.h>

namespace flutter {
class ExternalTextureSurfaceVulkanBufferDma
    : public ExternalTextureSurfaceVulkanBuffer {
 public:
  ExternalTextureSurfaceVulkanBufferDma(TizenRendererVulkan* vulkan_renderer);
  virtual ~ExternalTextureSurfaceVulkanBufferDma();
  bool CreateImage(tbm_surface_h tbm_surface) override;
  void ReleaseImage() override;
  bool AllocateMemory(tbm_surface_h tbm_surface) override;
  bool BindImageMemory(tbm_surface_h tbm_surface) override;
  uint64_t GetAllocSize() override;
  uint32_t GetFormatFeaturesProperties() override;
  VkFormat GetFormat() override;
  VkImage GetImage() override;
  VkDeviceMemory GetMemory() override;

 private:
  bool GetFormatModifierProperties(
      VkFormat format,
      VkDrmFormatModifierPropertiesEXT& properties);
  VkMemoryRequirements2 GetImageMemoryRequirements(
      VkImageAspectFlagBits aspect_flag);
  TizenRendererVulkan* vulkan_renderer_ = nullptr;
  VkFormat vk_format_ = VK_FORMAT_UNDEFINED;
  VkImage vk_image_ = VK_NULL_HANDLE;
  VkDeviceMemory vk_device_memory_ = VK_NULL_HANDLE;
};
}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_TIZEN_EXTERNAL_TEXTURE_SURFACE_VULKAN_BUFFER_DMA_H_
