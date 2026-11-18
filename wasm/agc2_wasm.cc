/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// C API for the WebAssembly build of AGC2.
//
// The JavaScript wrapper (`wasm/agc2.mjs`) is the intended interface; this
// layer only exists to cross the wasm boundary with plain C types.

#include <algorithm>
#include <cstddef>
#include <memory>

#include "api/audio_processing.h"
#include "api/environment.h"
#include "audio_processing/agc2/input_volume_controller.h"
#include "audio_processing/audio_buffer.h"
#include "audio_processing/gain_controller2.h"

namespace {

// Owns the AGC2 instance together with the 10 ms audio buffer it operates on.
class Agc2 {
 public:
  Agc2(int sample_rate_hz,
       int num_channels,
       bool enable_input_volume_controller,
       bool enable_adaptive_digital,
       float fixed_gain_db,
       bool use_internal_vad)
      : buffer_(sample_rate_hz,
                num_channels,
                sample_rate_hz,
                num_channels,
                sample_rate_hz) {
    config_.enabled = true;
    config_.input_volume_controller.enabled = enable_input_volume_controller;
    config_.adaptive_digital.enabled = enable_adaptive_digital;
    config_.fixed_digital.gain_db = fixed_gain_db;
    agc2_ = std::make_unique<webrtc::GainController2>(
        env_, config_, webrtc::InputVolumeController::Config{}, sample_rate_hz,
        num_channels, use_internal_vad);
  }

  webrtc::GainController2& agc2() { return *agc2_; }
  webrtc::AudioBuffer& buffer() { return buffer_; }
  const webrtc::AudioProcessing::Config::GainController2& config() const {
    return config_;
  }

 private:
  webrtc::Environment env_;
  webrtc::AudioProcessing::Config::GainController2 config_;
  webrtc::AudioBuffer buffer_;
  std::unique_ptr<webrtc::GainController2> agc2_;
};

}  // namespace

extern "C" {

// Creates an AGC2 instance. Returns null on invalid arguments.
//
// `sample_rate_hz` must produce an APM frame size (80, 160, 320 or 480
// samples per channel); `fixed_gain_db` must be in [0, 50).
Agc2* agc2_create(int sample_rate_hz,
                  int num_channels,
                  int enable_input_volume_controller,
                  int enable_adaptive_digital,
                  float fixed_gain_db,
                  int use_internal_vad) {
  if (sample_rate_hz <= 0 || num_channels <= 0 || fixed_gain_db < 0.0f ||
      fixed_gain_db >= 50.0f) {
    return nullptr;
  }
  const int frame_size = sample_rate_hz / 100;
  if (frame_size != 80 && frame_size != 160 && frame_size != 320 &&
      frame_size != 480) {
    return nullptr;
  }
  return new Agc2(sample_rate_hz, num_channels,
                  enable_input_volume_controller != 0,
                  enable_adaptive_digital != 0, fixed_gain_db,
                  use_internal_vad != 0);
}

void agc2_destroy(Agc2* handle) {
  delete handle;
}

// Number of samples per channel in a 10 ms frame.
int agc2_frame_size(int sample_rate_hz) {
  return sample_rate_hz / 100;
}

void agc2_set_fixed_gain_db(Agc2* handle, float gain_db) {
  if (handle) {
    handle->agc2().SetFixedGainDb(gain_db);
  }
}

void agc2_set_capture_output_used(Agc2* handle, int capture_output_used) {
  if (handle) {
    handle->agc2().SetCaptureOutputUsed(capture_output_used != 0);
  }
}

// Processes one 10 ms frame of interleaved audio in place.
//
// Samples use the FloatS16 scale ([-32768, 32768]) like WebRTC's
// `AudioBuffer`; `agc2_frame_size(sample_rate_hz) * num_channels` samples are
// read and written. `applied_input_volume` is the input volume currently
// applied by the capture device (0-255) and is only used when the input volume
// controller is enabled.
void agc2_process_frame(Agc2* handle,
                        float* interleaved,
                        int applied_input_volume,
                        int input_volume_changed) {
  if (!handle || !interleaved) {
    return;
  }
  webrtc::DeinterleavedView<float> view = handle->buffer().view();
  const size_t num_channels = view.num_channels();
  const size_t num_frames = view.samples_per_channel();

  for (size_t channel_index = 0; channel_index < num_channels;
       ++channel_index) {
    webrtc::MonoView<float> channel = view[channel_index];
    for (size_t i = 0; i < num_frames; ++i) {
      channel[i] = interleaved[i * num_channels + channel_index];
    }
  }

  if (handle->config().input_volume_controller.enabled) {
    handle->agc2().Analyze(std::clamp(applied_input_volume, 0, 255),
                           handle->buffer());
  }
  handle->agc2().Process(input_volume_changed != 0, &handle->buffer());

  for (size_t channel_index = 0; channel_index < num_channels;
       ++channel_index) {
    webrtc::MonoView<float> channel = view[channel_index];
    for (size_t i = 0; i < num_frames; ++i) {
      interleaved[i * num_channels + channel_index] = channel[i];
    }
  }
}

// Returns the recommended input volume, or -1 if there is no recommendation.
int agc2_recommended_input_volume(Agc2* handle) {
  if (!handle) {
    return -1;
  }
  const std::optional<int> volume = handle->agc2().recommended_input_volume();
  return volume.has_value() ? *volume : -1;
}

}  // extern "C"