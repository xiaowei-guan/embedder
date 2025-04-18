
// Copyright 2024 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_TIZEN_RENDERER_VULKAN_H_
#define EMBEDDER_TIZEN_RENDERER_VULKAN_H_

#include "flutter/shell/platform/tizen/tizen_renderer.h"
#include "flutter/shell/platform/tizen/tizen_view_base.h"

namespace flutter {

class TizenRendererVulkan : public TizenRenderer {
 public:
  explicit TizenRendererVulkan(TizenViewBase* view);
  virtual ~TizenRendererVulkan();

  bool CreateSurface(void* render_target,
                     void* render_target_display,
                     int32_t width,
                     int32_t height) override;

  void DestroySurface() override;

  void ResizeSurface(int32_t width, int32_t height) override;
};
}  // namespace flutter

#endif  // EMBEDDER_TIZEN_RENDERER_VULKAN_H_
