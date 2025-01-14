// Copyright 2024 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_TIZEN_EXTERNAL_TEXTURE_PIXEL_VULKAN_H_
#define FLUTTER_SHELL_PLATFORM_TIZEN_EXTERNAL_TEXTURE_PIXEL_VULKAN_H_

#include "flutter/shell/platform/tizen/external_texture_surface_vulkan.h"
#include "flutter/shell/platform/tizen/logger.h"

#ifndef DRM_FORMAT_MOD_LINEAR
#define DRM_FORMAT_MOD_LINEAR 0
#endif

namespace flutter {

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
ExternalTextureSurfaceVulkan::ExternalTextureSurfaceVulkan(
    FlutterDesktopGpuSurfaceTextureCallback texture_callback,
    void* user_data,
    TizenRendererVulkan* vulkan_renderer)
    : ExternalTexture(),
      texture_callback_(texture_callback),
      user_data_(user_data),
      vulkan_renderer_(vulkan_renderer) {}

ExternalTextureSurfaceVulkan::~ExternalTextureSurfaceVulkan() {}

bool ExternalTextureSurfaceVulkan::BindImageMemory(tbm_surface_h tbm_surface) {
  uint32_t num_bos =
      static_cast<uint32_t>(tbm_surface_internal_get_num_bos(tbm_surface));
  if (num_bos > 1) {
    return BindMultiImageMemory(tbm_surface);
  } else {
    return BindOneImageMemory(tbm_surface);
  }
}

bool ExternalTextureSurfaceVulkan::BindOneImageMemory(
    tbm_surface_h tbm_surface) {
  tbm_surface_info_s tbm_surface_info;
  tbm_surface_get_info(tbm_surface, &tbm_surface_info);
  return vkBindImageMemory(
             static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
             vk_image_, device_memories_[0], 0) == VK_SUCCESS;
}

bool ExternalTextureSurfaceVulkan::BindMultiImageMemory(
    tbm_surface_h tbm_surface) {
  tbm_surface_info_s tbm_surface_info;
  tbm_surface_get_info(tbm_surface, &tbm_surface_info);
  std::vector<VkBindImageMemoryInfo> bind_image_mem_infos(
      tbm_surface_info.num_planes);
  std::vector<VkBindImagePlaneMemoryInfo> bind_image_plane_mem_infos(
      tbm_surface_info.num_planes);
  const VkImageAspectFlagBits plane_flag_bits[4] = {
      VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT,
      VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT,
      VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT,
      VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT};
  for (uint32_t i = 0; i < tbm_surface_info.num_planes; ++i) {
    bind_image_plane_mem_infos[i].sType =
        VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO;
    bind_image_plane_mem_infos[i].pNext = nullptr;
    bind_image_plane_mem_infos[i].planeAspect = plane_flag_bits[i];

    bind_image_mem_infos[i].sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
    bind_image_mem_infos[i].pNext = &bind_image_plane_mem_infos[i];
    bind_image_mem_infos[i].image = vk_image_;
    bind_image_mem_infos[i].memory = device_memories_[i];
    bind_image_mem_infos[i].memoryOffset = tbm_surface_info.planes[i].offset;
  }
  return vkBindImageMemory2KHR(
             static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
             tbm_surface_info.num_planes,
             bind_image_mem_infos.data()) == VK_SUCCESS;
}

bool ExternalTextureSurfaceVulkan::CreateOrUpdateImage(
    const FlutterDesktopGpuSurfaceDescriptor* descriptor) {
  if (descriptor == nullptr || descriptor->handle == nullptr) {
    ReleaseImage();
    return false;
  }
  void* handle = descriptor->handle;
  if (handle != last_surface_handle_) {
    ReleaseImage();
    const tbm_surface_h tbm_surface =
        reinterpret_cast<tbm_surface_h>(descriptor->handle);
    tbm_surface_info_s tbm_surface_info;
    if (tbm_surface_get_info(tbm_surface, &tbm_surface_info) !=
        TBM_SURFACE_ERROR_NONE) {
      if (descriptor->release_callback) {
        descriptor->release_callback(descriptor->release_context);
      }
      return false;
    }
    if (!CreateImage(tbm_surface)) {
      FT_LOG(Error) << "Fail to create image";
      ReleaseImage();
      return false;
    }
    if (!AllocateMemory(tbm_surface)) {
      FT_LOG(Error) << "Fail to allocate memory";
      ReleaseImage();
      return false;
    }
    if (!BindImageMemory(tbm_surface)) {
      FT_LOG(Error) << "Fail to bind image memory";
      ReleaseImage();
      return false;
    }
    last_surface_handle_ = handle;
  }
  if (descriptor->release_callback) {
    descriptor->release_callback(descriptor->release_context);
  }
  return true;
}

bool ExternalTextureSurfaceVulkan::CreateImage(tbm_surface_h tbm_surface) {
  tbm_surface_info_s tbm_surface_info;
  tbm_surface_get_info(tbm_surface, &tbm_surface_info);
  format_ = ConvertFormat(tbm_surface_info.format);
  VkDrmFormatModifierPropertiesEXT drm_format_modifier;
  if (!GetFormatModifierProperties(format_, drm_format_modifier)) {
    FT_LOG(Info) << "Fail to get format modifier";
    return false;
  }
  std::vector<VkSubresourceLayout> plane_layout(
      drm_format_modifier.drmFormatModifierPlaneCount);
  for (uint32_t i = 0; i < tbm_surface_info.num_planes; ++i) {
    plane_layout[i].offset = tbm_surface_info.planes[i].offset;
    plane_layout[i].size = tbm_surface_info.planes[i].size;
    plane_layout[i].rowPitch = tbm_surface_info.planes[i].stride;
    plane_layout[i].arrayPitch = 0;
    plane_layout[i].depthPitch = 0;
  }
  VkImageDrmFormatModifierExplicitCreateInfoEXT
      image_drm_format_modifier_create_info = {};
  image_drm_format_modifier_create_info.sType =
      VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT;
  image_drm_format_modifier_create_info.drmFormatModifier =
      DRM_FORMAT_MOD_LINEAR;
  image_drm_format_modifier_create_info.drmFormatModifierPlaneCount =
      drm_format_modifier.drmFormatModifierPlaneCount;
  image_drm_format_modifier_create_info.pPlaneLayouts = plane_layout.data();

  VkExternalMemoryImageCreateInfoKHR external_memory_image_create_info = {};
  external_memory_image_create_info.sType =
      VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR;
  external_memory_image_create_info.pNext =
      &image_drm_format_modifier_create_info;
  external_memory_image_create_info.handleTypes =
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

  VkImageCreateInfo image_create_info = {};
  image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_create_info.pNext = &external_memory_image_create_info;
  image_create_info.flags = 0;
  if (SupportDisjoint()) {
    image_create_info.flags =
        VK_IMAGE_CREATE_DISJOINT_BIT;  // multi-planar format support
  }
  image_create_info.imageType = VK_IMAGE_TYPE_2D;
  image_create_info.format = format_;
  image_create_info.extent = {tbm_surface_info.width, tbm_surface_info.height,
                              1};
  image_create_info.mipLevels = 1;
  image_create_info.arrayLayers = 1;
  image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_create_info.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
  image_create_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
  image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_create_info.queueFamilyIndexCount = 0;
  image_create_info.pQueueFamilyIndices = nullptr;
  image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  if (vkCreateImage(static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
                    &image_create_info, nullptr, &vk_image_) != VK_SUCCESS) {
    FT_LOG(Error) << "Fail to create VkImage";
    return false;
  }
  return true;
}

bool ExternalTextureSurfaceVulkan::SupportDisjoint() {
  if (IsMultiPlanarVkFormat(format_)) {
    VkDrmFormatModifierPropertiesEXT props;
    if (GetFormatModifierProperties(format_, props) &&
        props.drmFormatModifierTilingFeatures &
            VK_FORMAT_FEATURE_DISJOINT_BIT) {
      return true;
    }
  }
  return false;
}

bool ExternalTextureSurfaceVulkan::AllocateMemory(tbm_surface_h tbm_surface) {
  int num_bos = tbm_surface_internal_get_num_bos(tbm_surface);
  for (int i = 0; i < num_bos; i++) {
    tbm_bo bo = tbm_surface_internal_get_bo(tbm_surface, i);
    int32_t fd =
        dup(static_cast<int32_t>(tbm_bo_get_handle(bo, TBM_DEVICE_3D).u32));
    VkDeviceSize import_size = static_cast<VkDeviceSize>(tbm_bo_size(bo));
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (!AllocateMemory(vk_image_, fd, import_size, memory)) {
      FT_LOG(Error) << "Fail to allocate memory";
      return false;
    }
    device_memories_.push_back(memory);
  }
  return true;
}

bool ExternalTextureSurfaceVulkan::AllocateMemory(VkImage image,
                                                  int fd,
                                                  VkDeviceSize import_size,
                                                  VkDeviceMemory& memory) {
  VkMemoryRequirements memory_requirements;
  VkMemoryFdPropertiesKHR memory_fd_properties = {};

  if (vkGetMemoryFdPropertiesKHR(
          static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
          VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd,
          &memory_fd_properties) != VK_SUCCESS) {
    FT_LOG(Error) << "Fail to get memory fd properties";
    return false;
  }
  vkGetImageMemoryRequirements(
      static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()), image,
      &memory_requirements);
  memory_requirements.memoryTypeBits = memory_fd_properties.memoryTypeBits;
  auto import_memory_fd_info = VkImportMemoryFdInfoKHR{};
  import_memory_fd_info.handleType =
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
  import_memory_fd_info.fd = fd;
  uint32_t memory_type_index;
  if (!FindMemoryType(memory_requirements.memoryTypeBits,
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
          &alloc_info, nullptr, &memory) != VK_SUCCESS) {
    FT_LOG(Error) << "Fail to allocate memory";
    return false;
  }
  return true;
}

void ExternalTextureSurfaceVulkan::ReleaseImage() {
  if (vk_image_ != VK_NULL_HANDLE) {
    vkDestroyImage(static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
                   vk_image_, nullptr);
    vk_image_ = VK_NULL_HANDLE;
  }

  for (size_t i = 0; i < device_memories_.size(); i++) {
    vkFreeMemory(static_cast<VkDevice>(vulkan_renderer_->GetDeviceHandle()),
                 device_memories_[i], nullptr);
  }
  device_memories_.clear();
}

bool ExternalTextureSurfaceVulkan::PopulateTexture(size_t width,
                                                   size_t height,
                                                   void* flutter_texture) {
  if (!texture_callback_) {
    return false;
  }
  const FlutterDesktopGpuSurfaceDescriptor* gpu_surface =
      texture_callback_(width, height, user_data_);
  if (!gpu_surface) {
    FT_LOG(Info) << "gpu_surface is null for texture ID: " << texture_id_;
    return false;
  }

  if (!CreateOrUpdateImage(gpu_surface)) {
    FT_LOG(Info) << "CreateOrUpdateEglImage fail for texture ID: "
                 << texture_id_;
    return false;
  }

  FlutterVulkanTexture* vulkan_texture =
      static_cast<FlutterVulkanTexture*>(flutter_texture);
  vulkan_texture->image = reinterpret_cast<uint64_t>(vk_image_);
  vulkan_texture->format = format_;
  vulkan_texture->width = width;
  vulkan_texture->height = height;
  return true;
}

bool ExternalTextureSurfaceVulkan::FindMemoryType(
    uint32_t type_filter,
    VkMemoryPropertyFlags properties,
    uint32_t& index_out) {
  VkPhysicalDeviceMemoryProperties memory_properties;
  vkGetPhysicalDeviceMemoryProperties(
      static_cast<VkPhysicalDevice>(
          vulkan_renderer_->GetPhysicalDeviceHandle()),
      &memory_properties);

  for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
    if ((type_filter & (1 << i)) &&
        (memory_properties.memoryTypes[i].propertyFlags & properties) ==
            properties) {
      index_out = i;
      return true;
    }
  }
  return false;
}

VkFormat ExternalTextureSurfaceVulkan::ConvertFormat(tbm_format& format) {
  switch (format) {
    case TBM_FORMAT_NV12:
    case TBM_FORMAT_NV21:
      return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
    case TBM_FORMAT_RGBA8888:
    case TBM_FORMAT_ARGB8888:
    case TBM_FORMAT_RGBX8888:
    case TBM_FORMAT_XRGB8888:
    case TBM_FORMAT_RGB888:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case TBM_FORMAT_BGR888:
    case TBM_FORMAT_XBGR8888:
    case TBM_FORMAT_BGRX8888:
    case TBM_FORMAT_ABGR8888:
    case TBM_FORMAT_BGRA8888:
      return VK_FORMAT_B8G8R8A8_UNORM;
    default:
      return VK_FORMAT_UNDEFINED;
  }
}

bool ExternalTextureSurfaceVulkan::GetFormatModifierProperties(
    VkFormat format,
    VkDrmFormatModifierPropertiesEXT& properties) {
  VkDrmFormatModifierPropertiesListEXT format_modifyer_properties_list = {};
  format_modifyer_properties_list.sType =
      VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;
  format_modifyer_properties_list.drmFormatModifierCount = 0;

  VkFormatProperties2KHR format_properties_2khr = {};
  format_properties_2khr.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2_KHR;
  format_properties_2khr.pNext = &format_modifyer_properties_list;

  vkGetPhysicalDeviceFormatProperties2KHR(
      static_cast<VkPhysicalDevice>(
          vulkan_renderer_->GetPhysicalDeviceHandle()),
      format, &format_properties_2khr);
  if (format_modifyer_properties_list.drmFormatModifierCount <= 0) {
    FT_LOG(Error) << "Could not get drmFormatModifierCount";
    return false;
  }
  std::vector<VkDrmFormatModifierPropertiesEXT> properties_list(
      format_modifyer_properties_list.drmFormatModifierCount);
  format_modifyer_properties_list.pDrmFormatModifierProperties =
      properties_list.data();
  vkGetPhysicalDeviceFormatProperties2KHR(
      static_cast<VkPhysicalDevice>(
          vulkan_renderer_->GetPhysicalDeviceHandle()),
      format, &format_properties_2khr);
  for (VkDrmFormatModifierPropertiesEXT& prop : properties_list) {
    if (prop.drmFormatModifier == DRM_FORMAT_MOD_LINEAR) {
      properties = prop;
      return true;
    }
  }
  return false;
}

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_TIZEN_EXTERNAL_TEXTURE_SURFACE_VULKAN_H_
