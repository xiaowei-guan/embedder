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
      static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()), vk_image_,
      &memory_requirements);
  FT_LOG(Error) << "GetAllocSize : " << memory_requirements.size;
  return memory_requirements.size;
}

uint32_t ExternalTextureSurfaceVulkanBufferDma::GetFormatFeaturesProperties() {
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(
      static_cast<VkPhysicalDevice>(
          vulkan_renderer_->GetPhysicalDeviceHandle()),
      vk_format_, &formatProperties);
  return formatProperties.linearTilingFeatures;
}

VkFormat ExternalTextureSurfaceVulkanBufferDma::GetFormat() {
  return vk_format_;
}

VkImage ExternalTextureSurfaceVulkanBufferDma::GetImage() {
  return vk_image_;
}

VkDeviceMemory ExternalTextureSurfaceVulkanBufferDma::GetMemory() {
  return vk_device_memory_;
}

bool IsMultiPlanarVkFormat(VkFormat format) {
  switch (format) {
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
    case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
      return true;

    default:
      return false;
  }
}

VkResult GetMemoryFdPropertiesKHR(
    VkInstance instance,
    VkDevice device,
    VkExternalMemoryHandleTypeFlagBits handleType,
    int fd,
    VkMemoryFdPropertiesKHR* pMemoryFdProperties) {
  auto func = (PFN_vkGetMemoryFdPropertiesKHR)vkGetInstanceProcAddr(
      instance, "vkGetMemoryFdPropertiesKHR");
  if (func != nullptr) {
    return func(device, handleType, fd, pMemoryFdProperties);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

bool ExternalTextureSurfaceVulkanBufferDma::CreateImage(
    tbm_surface_h tbm_surface) {
  FT_LOG(Error) << "CreateImage!";
  tbm_surface_info_s tbm_surface_info;
  tbm_surface_get_info(tbm_surface, &tbm_surface_info);
  vk_format_ = ConvertFormat(tbm_surface_info.format);
  FT_LOG(Error) << "format_ : " << vk_format_;

  // if(!GetFormatFeaturesProperties)

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

bool ExternalTextureSurfaceVulkanBufferDma::AllocateMemory(
    tbm_surface_h tbm_surface) {
  FT_LOG(Error) << "AllocateMemory!";
  tbm_bo bo = tbm_surface_internal_get_bo(tbm_surface, 0);
  int32_t fd =
      dup(static_cast<int32_t>(tbm_bo_get_handle(bo, TBM_DEVICE_3D).u32));
  VkMemoryRequirements memory_requirements;
  VkMemoryFdPropertiesKHR memory_fd_properties = {};

  if (GetMemoryFdPropertiesKHR(
          static_cast<VkInstance>(vulkan_renderer_->GetInstanceHandle()),
          static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
          VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd,
          &memory_fd_properties) != VK_SUCCESS) {
    FT_LOG(Error) << "Fail to get memory fd properties";
    return false;
  }
  vkGetImageMemoryRequirements(
      static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()), vk_image_,
      &memory_requirements);
  memory_requirements.memoryTypeBits = memory_fd_properties.memoryTypeBits;
  auto import_memory_fd_info = VkImportMemoryFdInfoKHR{};
  import_memory_fd_info.handleType =
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
  import_memory_fd_info.fd = fd;
  uint32_t memory_type_index;
  if (!FindProperties(memory_requirements.memoryTypeBits,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      memory_type_index)) {
    FT_LOG(Error) << "Fail to find memory type";
    return false;
  }

  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.pNext = &import_memory_fd_info;
  alloc_info.allocationSize = memory_requirements.size;
  alloc_info.memoryTypeIndex = memory_type_index;
  if (vkAllocateMemory(
          static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
          &alloc_info, nullptr, &vk_device_memory_) != VK_SUCCESS) {
    FT_LOG(Error) << "Fail to allocate memory";
    return false;
  }
  return true;
}

VkMemoryRequirements2
ExternalTextureSurfaceVulkanBufferDma::GetImageMemoryRequirements(
    VkImageAspectFlagBits aspect_flag) {
  VkImagePlaneMemoryRequirementsInfo image_plane_memory_info = {};
  image_plane_memory_info.sType =
      VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO;
  image_plane_memory_info.pNext = nullptr;
  image_plane_memory_info.planeAspect = aspect_flag;

  VkImageMemoryRequirementsInfo2 image_memory_info2 = {};
  image_memory_info2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
  image_memory_info2.pNext = &image_plane_memory_info;
  image_memory_info2.image = vk_image_;

  VkMemoryDedicatedRequirements dedicated_requirements = {};
  dedicated_requirements.sType =
      VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;

  VkMemoryRequirements2 memory_requirements2 = {};
  memory_requirements2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
  memory_requirements2.pNext = &dedicated_requirements;

  vkGetImageMemoryRequirements2(
      static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
      &image_memory_info2, &memory_requirements2);
  return memory_requirements2;
}

bool ExternalTextureSurfaceVulkanBufferDma::BindImageMemory(
    tbm_surface_h tbm_surface) {
  if (vkBindImageMemory(
          static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()), vk_image_,
          vk_device_memory_, 0u) != VK_SUCCESS) {
    FT_LOG(Error) << "Fail to bind image memory";
    return false;
  }
  return true;
}

void ExternalTextureSurfaceVulkanBufferDma::ReleaseImage() {
  FT_LOG(Error) << "ReleaseImage!";
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

bool ExternalTextureSurfaceVulkanBufferDma::GetFormatModifierProperties(
    VkFormat format,
    VkDrmFormatModifierPropertiesEXT& properties) {
  VkDrmFormatModifierPropertiesListEXT format_modifyer_properties_list = {};
  format_modifyer_properties_list.sType =
      VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;
  format_modifyer_properties_list.drmFormatModifierCount = 0;

  VkFormatProperties2 format_properties_2 = {};
  format_properties_2.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
  format_properties_2.pNext = &format_modifyer_properties_list;
  vkGetPhysicalDeviceFormatProperties2(
      static_cast<VkPhysicalDevice>(
          vulkan_renderer_->GetPhysicalDeviceHandle()),
      format, &format_properties_2);
  if (format_modifyer_properties_list.drmFormatModifierCount <= 0) {
    FT_LOG(Error) << "Could not get drmFormatModifierCount";
    return false;
  }
  std::vector<VkDrmFormatModifierPropertiesEXT> properties_list(
      format_modifyer_properties_list.drmFormatModifierCount);
  format_modifyer_properties_list.pDrmFormatModifierProperties =
      properties_list.data();
  vkGetPhysicalDeviceFormatProperties2(
      static_cast<VkPhysicalDevice>(
          vulkan_renderer_->GetPhysicalDeviceHandle()),
      format, &format_properties_2);
  for (VkDrmFormatModifierPropertiesEXT& prop : properties_list) {
    if (prop.drmFormatModifier == DRM_FORMAT_MOD_LINEAR) {
      properties = prop;
      return true;
    }
  }
  return false;
}

}  // namespace flutter
