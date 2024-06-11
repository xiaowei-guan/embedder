// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_PLATFORM_CHANNEL_H_
#define EMBEDDER_PLATFORM_CHANNEL_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "flutter/shell/platform/common/client_wrapper/include/flutter/binary_messenger.h"
#include "flutter/shell/platform/common/client_wrapper/include/flutter/method_channel.h"
#ifdef CLIPBOARD_SUPPORT
#include "flutter/shell/platform/tizen/tizen_clipboard.h"
#endif
#include "flutter/shell/platform/tizen/tizen_view_base.h"
#include "rapidjson/document.h"

namespace flutter {

class PlatformChannel {
 public:
  explicit PlatformChannel(BinaryMessenger* messenger, TizenViewBase* view);
  virtual ~PlatformChannel();

 private:
  using ClipboardCallback =
      std::function<void(std::optional<std::string> data)>;

  void HandleMethodCall(
      const MethodCall<rapidjson::Document>& call,
      std::unique_ptr<MethodResult<rapidjson::Document>> result);

  void SystemNavigatorPop();
  void PlaySystemSound(const std::string& sound_type);
  void HapticFeedbackVibrate(const std::string& feedback_type);
  bool GetClipboardData(ClipboardCallback on_data);
  void SetClipboardData(const std::string& data);
  bool ClipboardHasStrings();
  void RestoreSystemUiOverlays();
  void RequestAppExit(const std::string exit_type);
  void RequestAppExitSuccess(const rapidjson::Document* result);
  void SetEnabledSystemUiOverlays(const std::vector<std::string>& overlays);
  void SetPreferredOrientations(const std::vector<std::string>& orientations);

  std::unique_ptr<MethodChannel<rapidjson::Document>> channel_;

  // Whether or not initialization is complete from the framework.
  bool initialization_complete_ = false;

  // A reference to the native view managed by FlutterTizenView.
  TizenViewBase* view_ = nullptr;
#ifdef CLIPBOARD_SUPPORT
  std::unique_ptr<TizenClipboard> tizen_clipboard_;
#else
  // A container that holds clipboard data during the engine lifetime.
  std::string clipboard_;
#endif
};

}  // namespace flutter

#endif  // EMBEDDER_PLATFORM_CHANNEL_H_
