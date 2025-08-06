// Copyright 2025 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/tizen/external_texture_surface_vulkan_buffer_map.h"
#include "flutter/shell/platform/tizen/logger.h"

#include <tbm_bufmgr.h>
#include <tbm_surface_internal.h>
#include <tbm_type.h>
#include <tbm_type_common.h>

namespace flutter {

ExternalTextureSurfaceVulkanBufferMap::ExternalTextureSurfaceVulkanBufferMap(
    TizenRendererVulkan* vulkan_renderer)
    : ExternalTextureSurfaceVulkanBuffer(vulkan_renderer),
      vulkan_renderer_(vulkan_renderer) {}

ExternalTextureSurfaceVulkanBufferMap::
    ~ExternalTextureSurfaceVulkanBufferMap() {
  ReleaseImage();
}

bool ExternalTextureSurfaceVulkanBufferMap::CreateImage(
    tbm_surface_h tbm_surface) {
  tbm_surface_info_s tbm_surface_info;
  tbm_surface_get_info(tbm_surface, &tbm_surface_info);
  vk_format_ = ConvertFormat(tbm_surface_info.format);

  VkImageCreateInfo image_create_info = {};
  image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_create_info.imageType = VK_IMAGE_TYPE_2D;
  image_create_info.format = vk_format_;
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
  image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  if (vkCreateImage(static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
                    &image_create_info, nullptr, &vk_image_) != VK_SUCCESS) {
    FT_LOG(Error) << "Fail to create VkImage";
    return false;
  }
  return true;
}

void ExternalTextureSurfaceVulkanBufferMap::ReleaseImage() {
  if (vk_image_ != VK_NULL_HANDLE) {
    vkDestroyImage(static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
                   vk_image_, nullptr);
    vk_image_ = VK_NULL_HANDLE;
  }
  if (vk_device_memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
                 vk_device_memory_, nullptr);
    vk_device_memory_ = VK_NULL_HANDLE;
  }
}

bool ExternalTextureSurfaceVulkanBufferMap::AllocateMemory(
    tbm_surface_h tbm_surface) {
  VkMemoryRequirements memory_requirements;
  vkGetImageMemoryRequirements(
      static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()), vk_image_,
      &memory_requirements);
  uint32_t memory_type_index;
  if (!FindProperties(memory_requirements.memoryTypeBits,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      memory_type_index)) {
    return false;
  }
  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = memory_requirements.size;
  alloc_info.memoryTypeIndex = memory_type_index;
  if (vkAllocateMemory(
          static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
          &alloc_info, nullptr, &vk_device_memory_) != VK_SUCCESS) {
    FT_LOG(Error) << "Fail to allocate memory";
    return false;
  }
  if (vk_format_ == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM) {
    return MapNV12(memory_requirements, tbm_surface);
  } else {
    FT_LOG(Error) << "Not support format...";
    return false;
  }
}

bool ExternalTextureSurfaceVulkanBufferMap::IsYCbCrSupported() {
  VkFormatProperties format_properties;
  vkGetPhysicalDeviceFormatProperties(
      static_cast<VkPhysicalDevice>(
          vulkan_renderer_->GetPhysicalDeviceHandle()),
      vk_format_, &format_properties);
  auto lin_flags = format_properties.linearTilingFeatures;
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

bool ExternalTextureSurfaceVulkanBufferMap::MapNV12(
    VkMemoryRequirements& memory_requirements,
    tbm_surface_h tbm_surface) {
  tbm_surface_info_s tbm_surface_info;
  // tbm_surface_get_info(tbm_surface, &tbm_surface_info);
  if (tbm_surface_map(tbm_surface, TBM_SURF_OPTION_READ, &tbm_surface_info) !=
      TBM_SURFACE_ERROR_NONE) {
    FT_LOG(Error) << "Fail to map tbm surface";
    return false;
  }

  void* mapped_buffer;
  if (vkMapMemory(static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
                  vk_device_memory_, 0u, memory_requirements.size, 0u,
                  &mapped_buffer) != VK_SUCCESS) {
    FT_LOG(Error) << "Fail to map memory";
    return false;
  }

  // Write Y channel
  VkImageSubresource subresource;
  subresource.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT;
  subresource.mipLevel = 0;
  subresource.arrayLayer = 0;
  VkSubresourceLayout y_layout;
  vkGetImageSubresourceLayout(
      static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()), vk_image_,
      &subresource, &y_layout);
  FT_LOG(Error) << "y_layout offset : " << y_layout.offset
                << ", rowPitch : " << y_layout.rowPitch
                << ", size : " << y_layout.size;

  uint8_t* bufferData =
      reinterpret_cast<uint8_t*>(mapped_buffer) + y_layout.offset;
  for (size_t i = 0; i < tbm_surface_info.height; i++) {
    memcpy(bufferData + i * y_layout.rowPitch,
           tbm_surface_info.planes[0].ptr + tbm_surface_info.planes[0].offset +
               i * tbm_surface_info.planes[0].stride,
           tbm_surface_info.width * sizeof(uint8_t));
  }
  subresource.aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT;
  VkSubresourceLayout uv_layout;
  vkGetImageSubresourceLayout(
      static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()), vk_image_,
      &subresource, &uv_layout);
  bufferData = reinterpret_cast<uint8_t*>(mapped_buffer) + uv_layout.offset;
  for (size_t i = 0; i < tbm_surface_info.height / 2; i++) {
    memcpy(bufferData + i * uv_layout.rowPitch,
           tbm_surface_info.planes[1].ptr + tbm_surface_info.planes[1].offset +
               i * tbm_surface_info.planes[1].stride,
           tbm_surface_info.width * sizeof(uint8_t));
  }
  tbm_surface_unmap(tbm_surface);

  VkMappedMemoryRange flush_range;
  flush_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  flush_range.pNext = nullptr;
  flush_range.memory = vk_device_memory_;
  flush_range.offset = 0;
  flush_range.size = VK_WHOLE_SIZE;
  if (vkFlushMappedMemoryRanges(
          static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()), 1,
          &flush_range) != VK_SUCCESS) {
    FT_LOG(Error) << "Fail to flush mapped memory range";
    return false;
  }
  vkUnmapMemory(static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
                vk_device_memory_);
  return true;
}

bool ExternalTextureSurfaceVulkanBufferMap::BindImageMemory(
    tbm_surface_h tbm_surface) {
  if (vkBindImageMemory(
          static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()), vk_image_,
          vk_device_memory_, 0u) != VK_SUCCESS) {
    FT_LOG(Error) << "Fail to bind image memory";
    return false;
  }
  return true;
}

uint64_t ExternalTextureSurfaceVulkanBufferMap::GetAllocSize() {
  VkMemoryRequirements memory_requirements;
  vkGetImageMemoryRequirements(
      static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()), vk_image_,
      &memory_requirements);
  return memory_requirements.size;
}

uint32_t ExternalTextureSurfaceVulkanBufferMap::GetFormatFeaturesProperties() {
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(
      static_cast<VkPhysicalDevice>(
          vulkan_renderer_->GetPhysicalDeviceHandle()),
      vk_format_, &formatProperties);
  return formatProperties.linearTilingFeatures;
}

VkFormat ExternalTextureSurfaceVulkanBufferMap::GetFormat() {
  return vk_format_;
}

VkImage ExternalTextureSurfaceVulkanBufferMap::GetImage() {
  return vk_image_;
}

VkDeviceMemory ExternalTextureSurfaceVulkanBufferMap::GetMemory() {
  return vk_device_memory_;
}

}  // namespace flutter
