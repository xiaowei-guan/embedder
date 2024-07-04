// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_TIZEN_RENDERER_H_
#define EMBEDDER_TIZEN_RENDERER_H_

#include <cstdint>

namespace flutter {

class TizenRenderer {
 public:
  TizenRenderer() = default;

  virtual ~TizenRenderer() = default;

  virtual bool CreateSurface(void* render_target,
                             void* render_target_display,
                             int32_t width,
                             int32_t height) = 0;

  virtual void DestroySurface() = 0;

  bool IsValid() { return is_valid_; }

  virtual void ResizeSurface(int32_t width, int32_t height) = 0;

 protected:
  bool is_valid_ = false;
};

}  // namespace flutter

#endif  // EMBEDDER_TIZEN_RENDERER_H_
