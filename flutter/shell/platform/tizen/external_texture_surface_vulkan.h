// Copyright 2024 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_TIZEN_EXTERNAL_TEXTURE_SURFACE_VULKAN_H_
#define FLUTTER_SHELL_PLATFORM_TIZEN_EXTERNAL_TEXTURE_SURFACE_VULKAN_H_

#include <vulkan/vulkan.h>
#include "flutter/shell/platform/common/public/flutter_texture_registrar.h"
#include "flutter/shell/platform/embedder/embedder.h"
#include "flutter/shell/platform/tizen/external_texture.h"
#include "flutter/shell/platform/tizen/tizen_renderer_vulkan.h"

#include <tbm_bufmgr.h>
#include <tbm_surface.h>
#include <tbm_surface_internal.h>
#include <tbm_type.h>

namespace flutter {
class ExternalTextureSurfaceVulkan : public ExternalTexture {
 public:
  ExternalTextureSurfaceVulkan(
      FlutterDesktopGpuSurfaceTextureCallback texture_callback,
      void* user_data,
      TizenRendererVulkan* vulkan_renderer);

  virtual ~ExternalTextureSurfaceVulkan();

  bool PopulateTexture(size_t width,
                       size_t height,
                       void* flutter_texture) override;

 private:
  bool BindImageMemory(tbm_surface_h tbm_surface);
  bool BindMultiImageMemory(tbm_surface_h tbm_surface);
  bool BindOneImageMemory(tbm_surface_h tbm_surface);
  bool CreateOrUpdateImage(
      const FlutterDesktopGpuSurfaceDescriptor* descriptor);
  bool CreateImage(tbm_surface_h tbm_surface);
  VkFormat ConvertFormat(tbm_format& format);
  bool GetFormatModifierProperties(
      VkFormat format,
      VkDrmFormatModifierPropertiesEXT& properties);
  bool FindMemoryType(uint32_t typeFilter,
                      VkMemoryPropertyFlags properties,
                      uint32_t& index_out);
  void ReleaseImage();
  bool SupportDisjoint();
  bool AllocateMemory(tbm_surface_h tbm_surface);
  bool AllocateMemory(VkImage image,
                      int fd,
                      VkDeviceSize import_size,
                      VkDeviceMemory& memory);
  FlutterDesktopGpuSurfaceTextureCallback texture_callback_ = nullptr;
  void* user_data_ = nullptr;
  TizenRendererVulkan* vulkan_renderer_ = nullptr;
  void* last_surface_handle_ = nullptr;
  VkFormat format_ = VK_FORMAT_UNDEFINED;
  VkImage vk_image_ = VK_NULL_HANDLE;
  std::vector<VkDeviceMemory> device_memories_;
};
}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_TIZEN_EXTERNAL_TEXTURE_SURFACE_VULKAN_H_
