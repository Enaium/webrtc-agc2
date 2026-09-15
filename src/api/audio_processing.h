/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_PROCESSING_H_
#define API_AUDIO_PROCESSING_H_

#include <stddef.h>

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// The Audio Processing Module (APM) provides a collection of voice processing
// components designed for real-time communications software.
//
// This is the standalone extraction of the APM configuration interface: it
// holds the parameters of the processing pipeline, of the gain controllers and
// the stream configuration, which are the only parts of the interface used by
// this module.
class AudioProcessing {
 public:
  // The struct below constitutes the parameter scheme for the audio
  // processing.
  struct RTC_EXPORT Config {
    // Sets the properties of the audio processing pipeline.
    struct RTC_EXPORT Pipeline {
      // Ways to downmix a multi-channel track to mono.
      enum class DownmixMethod {
        kAverageChannels,  // Average across channels.
        kUseFirstChannel,  // Use the first channel.
        kAdaptive          // Adaptively choose how to downmix.
      };

      // Maximum allowed processing rate used internally. May only be set to
      // 32000 or 48000 and any differing values will be treated as 48000.
      int maximum_internal_processing_rate = 32000;
      // Allow multi-channel processing of render audio.
      bool multi_channel_render = true;
      // Allow multi-channel processing of capture audio when AEC3 is active
      // or a custom AEC is injected.
      bool multi_channel_capture = true;
      // Indicates how to downmix multi-channel capture audio to mono (when
      // needed).
      DownmixMethod capture_downmix_method = DownmixMethod::kAverageChannels;
      DownmixMethod capture_downmix_method_stereo_aec =
          DownmixMethod::kAverageChannels;
    } pipeline;

    // Enables automatic gain control (AGC) functionality.
    // The automatic gain control (AGC) component brings the signal to an
    // appropriate range. This is done by applying a digital gain directly and,
    // in the analog mode, prescribing an analog gain to be applied at the audio
    // HAL.
    // Recommended to be enabled on the client-side.
    struct RTC_EXPORT GainController1 {
      bool operator==(const GainController1& rhs) const;
      bool operator!=(const GainController1& rhs) const {
        return !(*this == rhs);
      }

      bool enabled = false;
      enum Mode {
        // Adaptive mode intended for use if an analog volume control is
        // available on the capture device. It will require the user to provide
        // coupling between the OS mixer controls and AGC through the
        // stream_analog_level() functions.
        // It consists of an analog gain prescription for the audio device and a
        // digital compression stage.
        kAdaptiveAnalog,
        // Adaptive mode intended for situations in which an analog volume
        // control is unavailable. It operates in a similar fashion to the
        // adaptive analog mode, but with scaling instead applied in the digital
        // domain. As with the analog mode, it additionally uses a digital
        // compression stage.
        kAdaptiveDigital,
        // Fixed mode which enables only the digital compression stage also used
        // by the two adaptive modes.
        // It is distinguished from the adaptive modes by considering only a
        // short time-window of the input signal. It applies a fixed gain
        // through most of the input level range, and compresses (gradually
        // reduces gain with increasing level) the input signal at higher
        // levels. This mode is preferred on embedded devices where the capture
        // signal level is predictable, so that a known gain can be applied.
        kFixedDigital
      };
      Mode mode = kAdaptiveAnalog;
      // Sets the target peak level (or envelope) of the AGC in dBFs (decibels
      // from digital full-scale). The convention is to use positive values. For
      // instance, passing in a value of 3 corresponds to -3 dBFs, or a target
      // level 3 dB below full-scale. Limited to [0, 31].
      int target_level_dbfs = 3;
      // Sets the maximum gain the digital compression stage may apply, in dB. A
      // higher number corresponds to greater compression, while a value of 0
      // will leave the signal uncompressed. Limited to [0, 90].
      // For updates after APM setup, use a RuntimeSetting instead.
      int compression_gain_db = 9;
      // When enabled, the compression stage will hard limit the signal to the
      // target level. Otherwise, the signal will be compressed but not limited
      // above the target level.
      bool enable_limiter = true;

      // Enables the analog gain controller functionality.
      struct AnalogGainController {
        bool enabled = true;
        // TODO(bugs.webrtc.org/7494): Deprecated. Stop using and remove.
        int startup_min_volume = 0;
        // Lowest analog microphone level that will be applied in response to
        // clipping.
        int clipped_level_min = 70;
        // If true, an adaptive digital gain is applied.
        bool enable_digital_adaptive = true;
        // Amount the microphone level is lowered with every clipping event.
        // Limited to (0, 255].
        int clipped_level_step = 15;
        // Proportion of clipped samples required to declare a clipping event.
        // Limited to (0.f, 1.f).
        float clipped_ratio_threshold = 0.1f;
        // Time in frames to wait after a clipping event before checking again.
        // Limited to values higher than 0.
        int clipped_wait_frames = 300;

        // Enables clipping prediction functionality.
        struct ClippingPredictor {
          bool enabled = false;
          enum Mode {
            // Clipping event prediction mode with fixed step estimation.
            kClippingEventPrediction,
            // Clipped peak estimation mode with adaptive step estimation.
            kAdaptiveStepClippingPeakPrediction,
            // Clipped peak estimation mode with fixed step estimation.
            kFixedStepClippingPeakPrediction,
          };
          Mode mode = kClippingEventPrediction;
          // Number of frames in the sliding analysis window.
          int window_length = 5;
          // Number of frames in the sliding reference window.
          int reference_window_length = 5;
          // Reference window delay (unit: number of frames).
          int reference_window_delay = 5;
          // Clipping prediction threshold (dBFS).
          float clipping_threshold = -1.0f;
          // Crest factor drop threshold (dB).
          float crest_factor_margin = 3.0f;
          // If true, the recommended clipped level step is used to modify the
          // analog gain. Otherwise, the predictor runs without affecting the
          // analog gain.
          bool use_predicted_step = true;
        } clipping_predictor;
      } analog_gain_controller;
    } gain_controller1;

    // Parameters for AGC2, an Automatic Gain Control (AGC) sub-module which
    // replaces the AGC sub-module parametrized by `gain_controller1`.
    // AGC2 brings the captured audio signal to the desired level by combining
    // three different controllers (namely, input volume controller, adapative
    // digital controller and fixed digital controller) and a limiter.
    // TODO(bugs.webrtc.org:7494): Name `GainController` when AGC1 removed.
    struct RTC_EXPORT GainController2 {
      bool operator==(const GainController2& rhs) const;
      bool operator!=(const GainController2& rhs) const {
        return !(*this == rhs);
      }

      // AGC2 must be created if and only if `enabled` is true.
      bool enabled = false;

      // Parameters for the input volume controller, which adjusts the input
      // volume applied when the audio is captured (e.g., microphone volume on
      // a soundcard, input volume on HAL).
      struct InputVolumeController {
        bool operator==(const InputVolumeController& rhs) const;
        bool operator!=(const InputVolumeController& rhs) const {
          return !(*this == rhs);
        }
        bool enabled = false;
      } input_volume_controller;

      // Parameters for the adaptive digital controller, which adjusts and
      // applies a digital gain after echo cancellation and after noise
      // suppression.
      struct RTC_EXPORT AdaptiveDigital {
        bool operator==(const AdaptiveDigital& rhs) const;
        bool operator!=(const AdaptiveDigital& rhs) const {
          return !(*this == rhs);
        }
        bool enabled = false;
        float headroom_db = 5.0f;
        float max_gain_db = 50.0f;
        float initial_gain_db = 15.0f;
        float max_gain_change_db_per_second = 6.0f;
        float max_output_noise_level_dbfs = -50.0f;
      } adaptive_digital;

      // Parameters for the fixed digital controller, which applies a fixed
      // digital gain after the adaptive digital controller and before the
      // limiter.
      struct FixedDigital {
        // By setting `gain_db` to a value greater than zero, the limiter can be
        // turned into a compressor that first applies a fixed gain.
        float gain_db = 0.0f;
      } fixed_digital;
    } gain_controller2;
  };

  // APM processes audio in chunks of about 10 ms.
  static constexpr int kChunkSizeMs = 10;

  // Returns floor(sample_rate_hz/100): the number of samples per channel used
  // as input and output to the audio processing module in calls to
  // ProcessStream, ProcessReverseStream, AnalyzeReverseStream, and
  // GetLinearAecOutput.
  //
  // This is exactly 10 ms for sample rates divisible by 100. For example:
  //  - 48000 Hz (480 samples per channel),
  //  - 44100 Hz (441 samples per channel),
  //  - 16000 Hz (160 samples per channel).
  static int GetFrameSize(int sample_rate_hz) { return sample_rate_hz / 100; }
};

// Settings for a stream: sample rate and number of channels.
class StreamConfig {
 public:
  // sample_rate_hz: The sampling rate of the stream.
  // num_channels: The number of audio channels in the stream.
  StreamConfig(int sample_rate_hz = 0,
               size_t num_channels = 0)  // NOLINT(runtime/explicit)
      : sample_rate_hz_(sample_rate_hz),
        num_channels_(num_channels),
        num_frames_(calculate_frames(sample_rate_hz)) {}

  void set_sample_rate_hz(int value) {
    sample_rate_hz_ = value;
    num_frames_ = calculate_frames(value);
  }
  void set_num_channels(size_t value) { num_channels_ = value; }

  int sample_rate_hz() const { return sample_rate_hz_; }

  // The number of channels in the stream.
  size_t num_channels() const { return num_channels_; }

  size_t num_frames() const { return num_frames_; }
  size_t num_samples() const { return num_channels_ * num_frames_; }

  bool operator==(const StreamConfig& other) const {
    return sample_rate_hz_ == other.sample_rate_hz_ &&
           num_channels_ == other.num_channels_;
  }

  bool operator!=(const StreamConfig& other) const { return !(*this == other); }

 private:
  static size_t calculate_frames(int sample_rate_hz) {
    return static_cast<size_t>(AudioProcessing::GetFrameSize(sample_rate_hz));
  }

  int sample_rate_hz_;
  size_t num_channels_;
  size_t num_frames_;
};

}  // namespace webrtc

#endif  // API_AUDIO_PROCESSING_H_