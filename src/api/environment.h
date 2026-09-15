/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ENVIRONMENT_H_
#define API_ENVIRONMENT_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/field_trials_view.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// A concrete FieldTrialsView implementation that returns empty strings for
// all lookups (i.e., no field trials are enabled).
class RTC_EXPORT NoOpFieldTrials : public FieldTrialsView {
 public:
  std::string Lookup(absl::string_view key) const override { return ""; }
  std::unique_ptr<FieldTrialsView> CreateCopy() const override {
    return std::make_unique<NoOpFieldTrials>();
  }
};

// Standalone environment: bundles the field trials view that the processing
// modules read their configuration from. When none is injected, a no-op view
// (no field trial enabled) is used.
class RTC_EXPORT Environment {
 public:
  Environment() : field_trials_(std::make_shared<NoOpFieldTrials>()) {}
  explicit Environment(std::unique_ptr<FieldTrialsView> field_trials)
      : field_trials_(std::move(field_trials)) {}

  Environment(const Environment&) = default;
  Environment(Environment&&) = default;
  Environment& operator=(const Environment&) = default;
  Environment& operator=(Environment&&) = default;
  ~Environment() = default;

  const FieldTrialsView& field_trials() const { return *field_trials_; }

 private:
  std::shared_ptr<FieldTrialsView> field_trials_;
};

}  // namespace webrtc

#endif  // API_ENVIRONMENT_H_