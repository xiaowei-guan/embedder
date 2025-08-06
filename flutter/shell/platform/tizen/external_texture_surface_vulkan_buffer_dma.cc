// Copyright 2025 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/tizen/external_texture_surface_vulkan_buffer_dma.h"
#include "flutter/shell/platform/tizen/logger.h"

#ifndef DRM_FORMAT_MOD_LINEAR
#define DRM_FORMAT_MOD_LINEAR 0
#endif

namespace flutter {

ExternalTextureSurfaceVulkanBufferDma::ExternalTextureSurfaceVulkanBufferDma(
    TizenRendererVulkan* vulkan_renderer)
    : ExternalTextureSurfaceVulkanBuffer(vulkan_renderer),
      vulkan_renderer_(vulkan_renderer) {}

ExternalTextureSurfaceVulkanBufferDma::
    ~ExternalTextureSurfaceVulkanBufferDma() {
  ReleaseImage();
}

uint64_t ExternalTextureSurfaceVulkanBufferDma::GetAllocSize() {
  VkMemoryRequirements memory_requirements;
  vkGetImageMemoryRequirements(
      static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
      texture_image_, &memory_requirements);
  return memory_requirements.size;
}

uint32_t ExternalTextureSurfaceVulkanBufferDma::GetFormatFeaturesProperties() {
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(
      static_cast<VkPhysicalDevice>(
          vulkan_renderer_->GetPhysicalDeviceHandle()),
      texture_format_, &formatProperties);
  return formatProperties.linearTilingFeatures;
}

VkFormat ExternalTextureSurfaceVulkanBufferDma::GetFormat() {
  return texture_format_;
}

VkImage ExternalTextureSurfaceVulkanBufferDma::GetImage() {
  return texture_image_;
}

VkDeviceMemory ExternalTextureSurfaceVulkanBufferDma::GetMemory() {
  return texture_device_memory_;
}

VkResult GetMemoryFdPropertiesKHR(
    VkDevice device,
    VkExternalMemoryHandleTypeFlagBits handleType,
    int fd,
    VkMemoryFdPropertiesKHR* pMemoryFdProperties) {
  auto func = (PFN_vkGetMemoryFdPropertiesKHR)vkGetDeviceProcAddr(
      device, "vkGetMemoryFdPropertiesKHR");
  if (func != nullptr) {
    return func(device, handleType, fd, pMemoryFdProperties);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

bool ExternalTextureSurfaceVulkanBufferDma::CreateImage(
    tbm_surface_h tbm_surface) {
  tbm_surface_info_s tbm_surface_info;
  tbm_surface_get_info(tbm_surface, &tbm_surface_info);
  texture_format_ = ConvertFormat(tbm_surface_info.format);

  VkExternalMemoryImageCreateInfoKHR external_image_create_info = {};
  external_image_create_info.sType =
      VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR;
  external_image_create_info.pNext = nullptr;
  external_image_create_info.handleTypes =
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

  VkImageCreateInfo image_create_info = {};
  image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_create_info.imageType = VK_IMAGE_TYPE_2D;
  image_create_info.format = texture_format_;
  image_create_info.extent = {tbm_surface_info.width, tbm_surface_info.height,
                              1};
  image_create_info.mipLevels = 1;
  image_create_info.arrayLayers = 1;
  image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_create_info.tiling = VK_IMAGE_TILING_LINEAR;
  image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_SAMPLED_BIT;
  image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_create_info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
  image_create_info.pNext = &external_image_create_info;
  if (vkCreateImage(static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
                    &image_create_info, nullptr,
                    &texture_image_) != VK_SUCCESS) {
    FT_LOG(Error) << "Fail to create VkImage";
    return false;
  }
  return true;
}

bool ExternalTextureSurfaceVulkanBufferDma::GetFdMemoryTypeIndex(
    int fd,
    uint32_t& index_out) {
  VkMemoryFdPropertiesKHR memory_fd_properties = {};
  memory_fd_properties.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;

  if (GetMemoryFdPropertiesKHR(
          static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
          VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd,
          &memory_fd_properties) != VK_SUCCESS) {
    FT_LOG(Error) << "Fail to get memory fd properties";
    return false;
  }

  for (uint32_t mem_idx = 0; mem_idx < VK_MAX_MEMORY_TYPES; mem_idx++) {
    if (memory_fd_properties.memoryTypeBits & (1 << mem_idx)) {
      index_out = mem_idx;
      return true;
    }
  }
  return false;
}

bool ExternalTextureSurfaceVulkanBufferDma::AllocateMemory(
    tbm_surface_h tbm_surface) {
  tbm_bo bo = tbm_surface_internal_get_bo(tbm_surface, 0);
  int bo_fd = tbm_bo_export_fd(bo);
  int bo_size = tbm_bo_size(bo);

  uint32_t memory_type_index = -1;
  if (!GetFdMemoryTypeIndex(bo_fd, memory_type_index)) {
    FT_LOG(Error) << "Fail to get memory type index";
    return false;
  }

  VkImportMemoryFdInfoKHR import_memory_fd_info = {};
  import_memory_fd_info.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
  import_memory_fd_info.handleType =
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
  import_memory_fd_info.fd = bo_fd;
  import_memory_fd_info.pNext = nullptr;

  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.pNext = &import_memory_fd_info;
  alloc_info.allocationSize = static_cast<uint64_t>(bo_size);
  alloc_info.memoryTypeIndex = memory_type_index;
  if (vkAllocateMemory(
          static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
          &alloc_info, nullptr, &texture_device_memory_) != VK_SUCCESS) {
    FT_LOG(Error) << "Fail to allocate memory";
    return false;
  }
  return true;
}

bool ExternalTextureSurfaceVulkanBufferDma::BindImageMemory(
    tbm_surface_h tbm_surface) {
  if (vkBindImageMemory(
          static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
          texture_image_, texture_device_memory_, 0u) != VK_SUCCESS) {
    FT_LOG(Error) << "Fail to bind image memory";
    return false;
  }
  return true;
}

void ExternalTextureSurfaceVulkanBufferDma::ReleaseImage() {
  if (texture_image_ != VK_NULL_HANDLE) {
    vkDestroyImage(static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
                   texture_image_, nullptr);
    texture_image_ = VK_NULL_HANDLE;
  }
  if (texture_device_memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
                 texture_device_memory_, nullptr);
    texture_device_memory_ = VK_NULL_HANDLE;
  }
}

bool ExternalTextureSurfaceVulkanBufferDma::IsYCbCrSupported() {
  VkFormatProperties format_properties;
  vkGetPhysicalDeviceFormatProperties(
      static_cast<VkPhysicalDevice>(
          vulkan_renderer_->GetPhysicalDeviceHandle()),
      texture_format_, &format_properties);
  auto lin_flags = format_properties.linearTilingFeatures;
  FT_LOG(Error) << "IsYCbCrSupported::lin_flags : " << lin_flags;
  if (!(lin_flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) ||
      !(lin_flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) ||
      !(lin_flags &
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT) ||
      !(lin_flags & VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT)) {
    // VK_FORMAT_G8_B8R8_2PLANE_420_UNORM is not supported
    return false;
  }
  return true;
}

void ExternalTextureSurfaceVulkanBufferDma::CheckFormat(VkFormat format) {
  VkFormatProperties properties;
  vkGetPhysicalDeviceFormatProperties(
      static_cast<VkPhysicalDevice>(
          vulkan_renderer_->GetPhysicalDeviceHandle()),
      format, &properties);

  if ((properties.linearTilingFeatures &
       (VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT |
        VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT)) == 0) {
    FT_LOG(Error) << "Linear doesn't support YCbCr conversions!";
  }
  if ((properties.linearTilingFeatures &
       VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT) == 0) {
    FT_LOG(Error) << "Linear doesn't support midpoint!";
  }
  if ((properties.linearTilingFeatures &
       VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT) == 0) {
    FT_LOG(Error) << "Linear doesn't support cosited!";
  }
  if ((properties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) ==
      0)
    FT_LOG(Error) << "Linear doesn't support sampling";
  if ((properties.linearTilingFeatures &
       VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) == 0)
    FT_LOG(Error) << "Linear doesn't support linear texture filtering";
  if ((properties.linearTilingFeatures & VK_FORMAT_FEATURE_DISJOINT_BIT) == 0) {
    FT_LOG(Error) << "Linear doesn't support disjoint planes";
  }

  if ((properties.optimalTilingFeatures &
       (VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT |
        VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT)) == 0) {
    FT_LOG(Error) << "Optimal doesn't support YCbCr conversions!";
  }
  if ((properties.optimalTilingFeatures &
       VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT) == 0) {
    FT_LOG(Error) << "Optimal doesn't support midpoint!";
  }
  if ((properties.optimalTilingFeatures &
       VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT) == 0) {
    FT_LOG(Error) << "Optimal doesn't support cosited!";
  }
  if ((properties.optimalTilingFeatures &
       VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)
    FT_LOG(Error) << "Optimal doesn't support sampling";
  if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DISJOINT_BIT) ==
      0) {
    FT_LOG(Error) << "Optimal doesn't support disjoint planes";
  }
}

}  // namespace flutter
