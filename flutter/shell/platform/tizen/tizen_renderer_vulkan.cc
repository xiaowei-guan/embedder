
// Copyright 2024 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/tizen/tizen_renderer_vulkan.h"
#include <vulkan/vulkan_wayland.h>
#include "flutter/shell/platform/tizen/logger.h"

namespace flutter {

const std::vector<const char*> validation_layers = {
    "VK_LAYER_KHRONOS_validation"};

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks* pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}

TizenRendererVulkan::TizenRendererVulkan(TizenViewBase* view) {
  InitVulkan();
  TizenRenderer::CreateSurface(view);
}

void TizenRendererVulkan::InitVulkan() {
  if (!CreateInstance()) {
    FT_LOG(Error) << "Failed to create Vulkan instance";
    return;
  }
  if (enable_validation_layers_) {
    SetupDebugMessenger();
  }
}

void TizenRendererVulkan::Cleanup() {
  if (enable_validation_layers_) {
    DestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
  }
  vkDestroyInstance(instance_, nullptr);
}

bool TizenRendererVulkan::CreateInstance() {
  if (enable_validation_layers_ && CheckValidationLayerSupport()) {
    FT_LOG(Error) << "Validation layers requested, but not available";
    return false;
  }

  if (!GetRequiredExtensions(instance_extensions_)) {
    FT_LOG(Error) << "Failed to get required extensions";
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
  VkInstanceCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledExtensionCount =
      static_cast<uint32_t>(instance_extensions_.size());
  create_info.ppEnabledExtensionNames = instance_extensions_.data();
  create_info.flags = 0;

  VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};

  if (enable_validation_layers_) {
    create_info.enabledLayerCount =
        static_cast<uint32_t>(validation_layers.size());
    create_info.ppEnabledLayerNames = validation_layers.data();
    PopulateDebugMessengerCreateInfo(debug_create_info);
    create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_create_info;
  } else {
    create_info.enabledLayerCount = 0;
    create_info.pNext = nullptr;
  }

  if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS) {
    FT_LOG(Error) << "Create instance failed.";
    return false;
  }
  return true;
}

bool TizenRendererVulkan::GetRequiredExtensions(
    std::vector<const char*>& extensions) {
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
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
      } else if (!strcmp(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
                         ext.extensionName)) {
        has_surface_extension = true;
        extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
      } else if (!strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                         ext.extensionName)) {
        if (enable_validation_layers_) {
          extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
      }
    }
  }

  if (!has_surface_extension) {
    FT_LOG(Error) << "vkEnumerateInstanceExtensionProperties failed to find "
                     "the extensions";
    return false;
  }

  return true;
}

void TizenRendererVulkan::SetupDebugMessenger() {
  VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
  PopulateDebugMessengerCreateInfo(debug_create_info);

  if (CreateDebugUtilsMessengerEXT(instance_, &debug_create_info, nullptr,
                                   &debug_messenger_) != VK_SUCCESS) {
    FT_LOG(Error) << "Failed to set up debug messenger";
  }
}

bool TizenRendererVulkan::CheckValidationLayerSupport() {
  uint32_t layer_count;
  if (vkEnumerateInstanceLayerProperties(&layer_count, nullptr)) {
    return false;
  }
  std::vector<VkLayerProperties> available_layers(layer_count);
  if (vkEnumerateInstanceLayerProperties(&layer_count,
                                         available_layers.data())) {
    return false;
  }

  for (const char* layer_name : validation_layers) {
    bool layer_found = false;
    for (const auto& layer_properties : available_layers) {
      if (strcmp(layer_name, layer_properties.layerName) == 0) {
        layer_found = true;
        break;
      }
    }
    if (!layer_found) {
      return false;
    }
  }
  return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
  FT_LOG(Error) << pCallbackData->pMessage;
  return VK_FALSE;
}

void TizenRendererVulkan::PopulateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
  createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = DebugMessengerCallback;
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
