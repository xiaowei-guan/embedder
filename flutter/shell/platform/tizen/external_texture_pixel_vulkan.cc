// Copyright 2024 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/tizen/external_texture_pixel_vulkan.h"

namespace flutter {
ExternalTexturePixelVulkan::ExternalTexturePixelVulkan(
    FlutterDesktopPixelBufferTextureCallback texture_callback,
    void* user_data)
    : ExternalTexture(),
      texture_callback_(texture_callback),
      user_data_(user_data) {}

bool ExternalTexturePixelVulkan::PopulateTexture(size_t width,
                                                 size_t height,
                                                 void* flutter_texture) {
  return true;
}

bool ExternalTexturePixelVulkan::CopyPixelBuffer(size_t& width,
                                                 size_t& height) {
  if (!texture_callback_) {
    return false;
  }

  const FlutterDesktopPixelBuffer* pixel_buffer =
      texture_callback_(width, height, user_data_);

  if (!pixel_buffer || !pixel_buffer->buffer) {
    return false;
  }
  return true;
}
}  // namespace flutter