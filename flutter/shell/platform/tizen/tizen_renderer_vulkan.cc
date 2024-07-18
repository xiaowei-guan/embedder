
// Copyright 2024 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/tizen/tizen_renderer_vulkan.h"
#include <Ecore_Wl2.h>
#include <vulkan/vulkan_wayland.h>
#include <optional>
#include "flutter/shell/platform/tizen/logger.h"

namespace flutter {

inline static void VK_CHECK_RESULT(VkResult result) {
  if (result != VK_SUCCESS) {
    FT_LOG(Error) << "VkResult is " << result << " in " << __FILE__
                  << " at line " << __LINE__;
  }
}

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

  if (!GetRequiredExtensions(enabled_instance_extensions_)) {
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
      static_cast<uint32_t>(enabled_instance_extensions_.size());
  create_info.ppEnabledExtensionNames = enabled_instance_extensions_.data();
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

bool TizenRendererVulkan::PickPhysicalDevice() {
  uint32_t gpu_count;
  VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance_, &gpu_count, nullptr));
  if (gpu_count <= 0) {
    FT_LOG(Error) << "No GPUs found";
    return false;
  }
  std::vector<VkPhysicalDevice> physical_devices(gpu_count);
  VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance_, &gpu_count,
                                             physical_devices.data()));
  uint32_t selected_score = 0;
  for (const auto& physical_device : physical_devices) {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    vkGetPhysicalDeviceFeatures(physical_device, &features);
    FT_LOG(Info) << "Device Name: " << properties.deviceName;
    uint32_t score = 0;
    std::vector<const char*> supported_extensions;
    uint32_t qfp_count;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &qfp_count,
                                             nullptr);
    std::vector<VkQueueFamilyProperties> qfp(qfp_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &qfp_count,
                                             qfp.data());
    std::optional<uint32_t> graphics_queue_family;
    for (uint32_t i = 0; i < qfp.size(); i++) {
      // Only pick graphics queues that can also present to the surface.
      // Graphics queues that can't present are rare if not nonexistent, but
      // the spec allows for this, so check it anyhow.
      VkBool32 surface_present_supported;
      VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceSupportKHR(
          physical_device, i, surface_, &surface_present_supported));

      if (!graphics_queue_family.has_value() &&
          qfp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
          surface_present_supported) {
        graphics_queue_family = i;
      }
      // Skip physical devices that don't have a graphics queue.
      if (!graphics_queue_family.has_value()) {
        FT_LOG(Info) << "Skipping due to no suitable graphics queues.";
        continue;
      }

      // Prefer discrete GPUs.
      if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1 << 30;
      }
      uint32_t extension_count;
      VK_CHECK_RESULT(vkEnumerateDeviceExtensionProperties(
          physical_device, nullptr, &extension_count, nullptr));
      std::vector<VkExtensionProperties> available_extensions(extension_count);
      VK_CHECK_RESULT(vkEnumerateDeviceExtensionProperties(
          physical_device, nullptr, &extension_count,
          available_extensions.data()));

      bool supports_swapchain = false;
      for (const auto& available_extension : available_extensions) {
        if (strcmp(VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                   available_extension.extensionName) == 0) {
          supports_swapchain = true;
          supported_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        }
        // The spec requires VK_KHR_portability_subset be enabled whenever it's
        // available on a device. It's present on compatibility ICDs like
        // MoltenVK.
        else if (strcmp("VK_KHR_portability_subset",
                        available_extension.extensionName) == 0) {
          supported_extensions.push_back("VK_KHR_portability_subset");
        }
        // Prefer GPUs that support VK_KHR_get_memory_requirements2.
        else if (strcmp(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
                        available_extension.extensionName) == 0) {
          score += 1 << 29;
          supported_extensions.push_back(
              VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
        }
      }
      // Skip physical devices that don't have swapchain support.
      if (!supports_swapchain) {
        FT_LOG(Info) << "Skipping due to lack of swapchain support.";
        continue;
      }
      // Prefer GPUs with larger max texture sizes.
      score += properties.limits.maxImageDimension2D;
      if (selected_score < score) {
        FT_LOG(Info) << "This is the best device so far. Score: " << score;

        selected_score = score;
        physical_device_ = physical_device;
        enabled_device_extensions_ = supported_extensions;
        graphics_queue_family_index_ = graphics_queue_family.value_or(
            std::numeric_limits<uint32_t>::max());
      }
    }
  }
  return physical_device_ != VK_NULL_HANDLE;
}

TizenRendererVulkan::~TizenRendererVulkan() {}
bool TizenRendererVulkan::CreateSurface(void* render_target,
                                        void* render_target_display,
                                        int32_t width,
                                        int32_t height) {
  VkWaylandSurfaceCreateInfoKHR createInfo;
  memset(&createInfo, 0, sizeof(createInfo));
  createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
  createInfo.pNext = NULL;
  createInfo.flags = 0;
  createInfo.display = static_cast<wl_display*>(render_target_display);
  createInfo.surface = static_cast<wl_surface*>(render_target);

  PFN_vkCreateWaylandSurfaceKHR vkCreateWaylandSurfaceKHR;
  vkCreateWaylandSurfaceKHR =
      (PFN_vkCreateWaylandSurfaceKHR)vkGetInstanceProcAddr(
          instance_, "vkCreateWaylandSurfaceKHR");

  if (!vkCreateWaylandSurfaceKHR) {
    FT_LOG(Error) << "Wayland: Vulkan instance missing "
                     "VK_KHR_wayland_surface extension";
    return false;
  }

  if (!vkCreateWaylandSurfaceKHR(instance_, &createInfo, NULL, &surface_)) {
    FT_LOG(Error) << "Failed to create surface.";
    return false;
  }
  return true;
}

bool TizenRendererVulkan::CreateLogicalDevice() {
  float priority = 1.0f;
  VkDeviceQueueCreateInfo queue_info{};
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.queueFamilyIndex = graphics_queue_family_index_;
  queue_info.queueCount = 1;
  queue_info.pQueuePriorities = &priority;

  VkPhysicalDeviceFeatures device_features{};
  VkDeviceCreateInfo device_info{};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.queueCreateInfoCount = 1;
  device_info.pQueueCreateInfos = &queue_info;
  device_info.enabledExtensionCount =
      static_cast<uint32_t>(enabled_device_extensions_.size());
  device_info.ppEnabledExtensionNames = enabled_device_extensions_.data();
  device_info.pEnabledFeatures = &device_features;

  vkCreateDevice(physical_device_, &device_info, nullptr, &logical_device_);

  vkGetDeviceQueue(logical_device_, graphics_queue_family_index_, 0,
                   &graphics_queue_);
  return true;
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
  return reinterpret_cast<void*>(
      vkGetInstanceProcAddr(reinterpret_cast<VkInstance>(instance), name));
}

FlutterVulkanImage TizenRendererVulkan::GetNextImage(
    const FlutterFrameInfo* frameInfo) {
  return FlutterVulkanImage();
}

bool TizenRendererVulkan::Present(const FlutterVulkanImage* image) {
  return false;
}

}  // namespace flutter
