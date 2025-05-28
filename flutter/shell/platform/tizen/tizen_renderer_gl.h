// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_TIZEN_RENDERER_GL_H_
#define EMBEDDER_TIZEN_RENDERER_GL_H_

#include <cstdint>
#include "flutter/shell/platform/tizen/tizen_renderer.h"

namespace flutter {

class TizenRendererGL : public TizenRenderer {
 public:
  TizenRendererGL() = default;

  virtual ~TizenRendererGL() = default;

  virtual bool OnMakeCurrent() = 0;

  virtual bool OnClearCurrent() = 0;

  virtual bool OnMakeResourceCurrent() = 0;

  virtual bool OnPresent() = 0;

  virtual uint32_t OnGetFBO() = 0;

  virtual void* OnProcResolver(const char* name) = 0;

  virtual bool IsSupportedExtension(const char* name) = 0;
};

}  // namespace flutter

#endif  // EMBEDDER_TIZEN_RENDERER_GL_H_
