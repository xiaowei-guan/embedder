
// Copyright 2024 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/tizen/tizen_renderer_vulkan.h"

namespace flutter {
TizenRendererVulkan::TizenRendererVulkan(TizenViewBase* view) {
  Create(view);
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

}  // namespace flutter
