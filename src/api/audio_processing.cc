/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_processing.h"

namespace webrtc {

using Agc2Config = AudioProcessing::Config::GainController2;

bool Agc2Config::AdaptiveDigital::operator==(
    const Agc2Config::AdaptiveDigital& rhs) const {
  return enabled == rhs.enabled && headroom_db == rhs.headroom_db &&
         max_gain_db == rhs.max_gain_db &&
         initial_gain_db == rhs.initial_gain_db &&
         max_gain_change_db_per_second == rhs.max_gain_change_db_per_second &&
         max_output_noise_level_dbfs == rhs.max_output_noise_level_dbfs;
}

bool Agc2Config::InputVolumeController::operator==(
    const Agc2Config::InputVolumeController& rhs) const {
  return enabled == rhs.enabled;
}

bool Agc2Config::operator==(const Agc2Config& rhs) const {
  return enabled == rhs.enabled &&
         fixed_digital.gain_db == rhs.fixed_digital.gain_db &&
         adaptive_digital == rhs.adaptive_digital &&
         input_volume_controller == rhs.input_volume_controller;
}

}  // namespace webrtc