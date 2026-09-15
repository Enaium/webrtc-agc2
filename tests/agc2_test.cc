/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Smoke test for the standalone AGC2 extraction.
//
// Scenarios:
//   1. Fixed digital gain + limiter through `GainController2`.
//   2. Input volume controller (clipping detection) through `GainController2`.
//   3. Adaptive digital gain controller (gain adaptation, noise limiting and
//      adaptation rate).
//   4. Internal voice activity detector (RNN VAD weights + inference).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <vector>

#include "api/audio_processing.h"
#include "api/environment.h"
#include "audio_processing/agc2/adaptive_digital_gain_controller.h"
#include "audio_processing/agc2/cpu_features.h"
#include "audio_processing/agc2/input_volume_controller.h"
#include "audio_processing/agc2/vad_wrapper.h"
#include "audio_processing/audio_buffer.h"
#include "audio_processing/gain_controller2.h"
#include "audio_processing/logging/apm_data_dumper.h"

namespace {

constexpr int kSampleRateHz = 16000;
constexpr int kChannels = 1;
constexpr int kFrameSamples = 160;  // 10 ms at 16 kHz.
constexpr float kMaxAbsFloatS16 = 32768.0f;

// ---------------------------------------------------------------------------
// Signal generation (FloatS16 scale: +/-32768 corresponds to 0 dBFS)
// ---------------------------------------------------------------------------

// A sine wave with the given peak amplitude (FloatS16 units).
std::vector<float> GenerateSine(float peak_amplitude,
                                float frequency_hz,
                                size_t num_samples) {
  std::vector<float> samples(num_samples);
  const double step = 2.0 * std::numbers::pi * frequency_hz / kSampleRateHz;
  for (size_t i = 0; i < num_samples; ++i) {
    samples[i] = peak_amplitude * std::sin(step * static_cast<double>(i));
  }
  return samples;
}

// A sine wave driven into hard clipping, so that a large fraction of the
// samples sits at full scale.
std::vector<float> GenerateClippedSine(float frequency_hz, size_t num_samples) {
  std::vector<float> samples = GenerateSine(2.0f * kMaxAbsFloatS16, frequency_hz,
                                            num_samples);
  for (float& sample : samples) {
    sample = std::clamp(sample, -kMaxAbsFloatS16, kMaxAbsFloatS16 - 1.0f);
  }
  return samples;
}

// A voiced-speech-like signal: a pulse train with harmonics shaped by a
// formant envelope and a syllable envelope.
std::vector<float> GenerateSpeechLike(float peak_amplitude,
                                      size_t num_samples) {
  std::vector<float> samples(num_samples, 0.0f);
  constexpr float kPitchHz = 120.0f;
  constexpr float kSyllableRateHz = 3.0f;
  // Harmonic amplitudes roughly following a vowel-like spectral envelope.
  constexpr float kHarmonicAmplitudes[] = {1.0f,   0.7f,  0.5f, 0.4f, 0.35f,
                                           0.3f,   0.25f, 0.2f, 0.15f};
  constexpr size_t kNumHarmonics = std::size(kHarmonicAmplitudes);
  for (size_t i = 0; i < num_samples; ++i) {
    const double t = static_cast<double>(i) / kSampleRateHz;
    double value = 0.0;
    for (size_t h = 0; h < kNumHarmonics; ++h) {
      value += kHarmonicAmplitudes[h] *
               std::sin(2.0 * std::numbers::pi * kPitchHz * (h + 1) * t);
    }
    // Syllable envelope in [0, 1].
    const double envelope =
        0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * kSyllableRateHz * t));
    samples[i] = static_cast<float>(value * envelope) * peak_amplitude / 2.5f;
  }
  return samples;
}

std::vector<float> GenerateSilence(size_t num_samples) {
  return std::vector<float>(num_samples, 0.0f);
}

// ---------------------------------------------------------------------------
// Level measurement
// ---------------------------------------------------------------------------

double RmsDbfs(const float* samples, size_t num_samples) {
  double sum = 0.0;
  for (size_t i = 0; i < num_samples; ++i) {
    sum += static_cast<double>(samples[i]) * samples[i];
  }
  const double rms = std::sqrt(sum / static_cast<double>(num_samples));
  return 20.0 * std::log10(std::max(rms, 1e-10) / kMaxAbsFloatS16);
}

double PeakDbfs(const float* samples, size_t num_samples) {
  double peak = 0.0;
  for (size_t i = 0; i < num_samples; ++i) {
    peak = std::max(peak, std::abs(static_cast<double>(samples[i])));
  }
  return 20.0 * std::log10(std::max(peak, 1e-10) / kMaxAbsFloatS16);
}

// ---------------------------------------------------------------------------
// AudioBuffer helpers
// ---------------------------------------------------------------------------

void WriteFrame(webrtc::AudioBuffer* buffer, const float* samples) {
  webrtc::MonoView<float> channel = buffer->view()[0];
  for (size_t i = 0; i < channel.size(); ++i) {
    channel[i] = samples[i];
  }
}

void ReadFrame(const webrtc::AudioBuffer& buffer, float* samples) {
  const float* const* channels = buffer.channels_const();
  for (size_t i = 0; i < buffer.num_frames(); ++i) {
    samples[i] = channels[0][i];
  }
}

// ---------------------------------------------------------------------------
// Scenario 1: fixed digital gain and limiter
// ---------------------------------------------------------------------------

bool TestFixedDigitalGainAndLimiter() {
  std::cout << "--- Scenario 1: fixed digital gain and limiter ---"
            << std::endl;

  webrtc::Environment env;
  webrtc::AudioProcessing::Config::GainController2 config;
  config.enabled = true;
  config.fixed_digital.gain_db = 0.0f;

  webrtc::GainController2 agc2(env, config, webrtc::InputVolumeController::Config{},
                               kSampleRateHz, kChannels, /*use_internal_vad=*/false);
  webrtc::AudioBuffer buffer(kSampleRateHz, kChannels, kSampleRateHz, kChannels,
                             kSampleRateHz);

  constexpr size_t kFramesPerPhase = 50;  // 0.5 s.
  constexpr float kInputPeak = 16384.0f;  // -6 dBFS.
  const std::vector<float> input =
      GenerateSine(kInputPeak, 1000.0f, kFramesPerPhase * kFrameSamples);
  const double input_rms_dbfs = RmsDbfs(input.data(), input.size());
  const double input_peak_dbfs = PeakDbfs(input.data(), input.size());

  std::vector<float> output(input.size());
  for (size_t f = 0; f < kFramesPerPhase; ++f) {
    WriteFrame(&buffer, input.data() + f * kFrameSamples);
    agc2.Process(/*input_volume_changed=*/false, &buffer);
    ReadFrame(buffer, output.data() + f * kFrameSamples);
  }
  const double unity_gain_rms_dbfs = RmsDbfs(output.data(), output.size());
  const double unity_gain_peak_dbfs = PeakDbfs(output.data(), output.size());

  std::cout << std::fixed << std::setprecision(2)
            << "  fixed gain   0 dB | input  : RMS " << input_rms_dbfs
            << " dBFS, peak " << input_peak_dbfs << " dBFS" << std::endl;
  std::cout << "  fixed gain   0 dB | output : RMS " << unity_gain_rms_dbfs
            << " dBFS, peak " << unity_gain_peak_dbfs << " dBFS" << std::endl;

  const bool unity_gain_ok = unity_gain_peak_dbfs <= 0.0 &&
                             std::abs(unity_gain_rms_dbfs - input_rms_dbfs) < 0.5;

  agc2.SetFixedGainDb(20.0f);
  for (size_t f = 0; f < kFramesPerPhase; ++f) {
    WriteFrame(&buffer, input.data() + f * kFrameSamples);
    agc2.Process(/*input_volume_changed=*/false, &buffer);
    ReadFrame(buffer, output.data() + f * kFrameSamples);
  }
  const double boosted_rms_dbfs = RmsDbfs(output.data(), output.size());
  const double boosted_peak_dbfs = PeakDbfs(output.data(), output.size());

  std::cout << "  fixed gain  20 dB | output : RMS " << boosted_rms_dbfs
            << " dBFS, peak " << boosted_peak_dbfs << " dBFS" << std::endl;

  const bool boosted_ok = boosted_peak_dbfs <= 0.0 &&
                          boosted_rms_dbfs - input_rms_dbfs > 3.0 &&
                          boosted_peak_dbfs > -3.0;

  std::cout << "  unity gain transparent : " << (unity_gain_ok ? "PASS" : "FAIL")
            << std::endl;
  std::cout << "  +20 dB limited to 0 dBFS : " << (boosted_ok ? "PASS" : "FAIL")
            << std::endl;
  return unity_gain_ok && boosted_ok;
}

// ---------------------------------------------------------------------------
// Scenario 2: input volume controller
// ---------------------------------------------------------------------------

bool TestInputVolumeController() {
  std::cout << "--- Scenario 2: input volume controller ---" << std::endl;

  webrtc::Environment env;
  webrtc::AudioProcessing::Config::GainController2 config;
  config.enabled = true;
  config.input_volume_controller.enabled = true;
  config.adaptive_digital.enabled = false;

  webrtc::InputVolumeController::Config input_volume_config;
  webrtc::GainController2 agc2(env, config, input_volume_config, kSampleRateHz,
                               kChannels, /*use_internal_vad=*/false);
  webrtc::AudioBuffer buffer(kSampleRateHz, kChannels, kSampleRateHz, kChannels,
                             kSampleRateHz);

  constexpr int kInitialInputVolume = 255;
  constexpr size_t kFrames = 500;  // 5 s.
  const std::vector<float> input =
      GenerateClippedSine(1000.0f, kFrames * kFrameSamples);

  // The caller applies the recommended input volume, as the audio HAL would.
  int applied_input_volume = kInitialInputVolume;
  std::vector<int> volume_trace{applied_input_volume};
  for (size_t f = 0; f < kFrames; ++f) {
    WriteFrame(&buffer, input.data() + f * kFrameSamples);
    agc2.Analyze(applied_input_volume, buffer);
    agc2.Process(/*input_volume_changed=*/false, &buffer);
    if (agc2.recommended_input_volume().has_value()) {
      applied_input_volume = *agc2.recommended_input_volume();
    }
    volume_trace.push_back(applied_input_volume);
  }

  const int final_volume = volume_trace.back();
  const int min_volume =
      *std::min_element(volume_trace.begin(), volume_trace.end());
  std::cout << "  initial input volume      : " << kInitialInputVolume
            << std::endl;
  std::cout << "  final input volume        : " << final_volume << std::endl;
  std::cout << "  minimum input volume      : " << min_volume << std::endl;

  const bool clipped_ok = final_volume < kInitialInputVolume;
  const bool bounded_ok = min_volume >= input_volume_config.min_input_volume;
  const bool step_ok =
      (kInitialInputVolume - min_volume) % input_volume_config.clipped_level_step == 0;
  std::cout << "  clipping lowers the volume : " << (clipped_ok ? "PASS" : "FAIL")
            << std::endl;
  std::cout << "  volume stays above minimum : " << (bounded_ok ? "PASS" : "FAIL")
            << std::endl;
  std::cout << "  volume lowered by steps    : " << (step_ok ? "PASS" : "FAIL")
            << std::endl;
  return clipped_ok && bounded_ok && step_ok;
}

// ---------------------------------------------------------------------------
// Scenario 3: adaptive digital gain controller
// ---------------------------------------------------------------------------

bool TestAdaptiveDigitalGainController() {
  std::cout << "--- Scenario 3: adaptive digital gain controller ---"
            << std::endl;

  webrtc::AudioProcessing::Config::GainController2::AdaptiveDigital config;
  config.enabled = true;

  webrtc::ApmDataDumper data_dumper(/*instance_index=*/1);
  webrtc::AdaptiveDigitalGainController controller(
      &data_dumper, config, /*adjacent_speech_frames_threshold=*/12);

  constexpr float kNoiseRmsDbfs = -70.0f;
  constexpr float kHeadroomDb = 20.0f;
  constexpr size_t kFramesPerPhase = 400;  // 4 s.
  constexpr size_t kRampFrames = 40;

  // The gain is limited by the noise floor: `max_output_noise_level_dbfs`
  // (-50 dBFS) minus the noise level (-70 dBFS) gives a 20 dB ceiling.
  const double noise_limited_gain_db =
      config.max_output_noise_level_dbfs - kNoiseRmsDbfs;

  std::vector<float> frame(kFrameSamples, 0.0f);
  std::vector<float*> channels{frame.data()};
  webrtc::DeinterleavedView<float> view(channels, kFrameSamples);

  // Processes `kFramesPerPhase` frames of a 1 kHz tone with the given speech
  // level and returns the gain applied to the last frame.
  const auto run_phase = [&](float speech_level_dbfs, bool measure_ramp,
                             double* max_step_db) {
    // Sine with an RMS level equal to `speech_level_dbfs`.
    const float input_peak =
        static_cast<float>(std::numbers::sqrt2) * kMaxAbsFloatS16 *
        std::pow(10.0f, speech_level_dbfs / 20.0f);
    const webrtc::AdaptiveDigitalGainController::FrameInfo info{
        .speech_probability = 1.0f,
        .speech_level_dbfs = speech_level_dbfs,
        .speech_level_reliable = true,
        .noise_rms_dbfs = kNoiseRmsDbfs,
        .headroom_db = kHeadroomDb,
        .limiter_envelope_dbfs = -100.0f};
    double last_gain_db = 0.0;
    double previous_gain_db = 0.0;
    double input_rms_dbfs = 0.0;
    for (size_t f = 0; f < kFramesPerPhase; ++f) {
      // Restore the input level: the controller applies the gain in place.
      for (size_t i = 0; i < kFrameSamples; ++i) {
        frame[i] = input_peak *
                   std::sin(2.0 * std::numbers::pi * 1000.0 * i / kSampleRateHz);
      }
      input_rms_dbfs = RmsDbfs(frame.data(), kFrameSamples);
      controller.Process(info, view);
      last_gain_db = RmsDbfs(frame.data(), kFrameSamples) - input_rms_dbfs;
      if (measure_ramp && f >= kRampFrames) {
        *max_step_db =
            std::max(*max_step_db, std::abs(last_gain_db - previous_gain_db));
      }
      previous_gain_db = last_gain_db;
      if (f % 100 == 0 || f + 1 == kFramesPerPhase) {
        std::cout << std::fixed << std::setprecision(2) << "  frame "
                  << std::setw(3) << f << " | speech level " << speech_level_dbfs
                  << " dBFS | gain " << last_gain_db << " dB" << std::endl;
      }
    }
    return last_gain_db;
  };

  // Phase 1: low speech level, the gain ramps up to the noise limited ceiling.
  double max_increase_step_db = 0.0;
  const double low_level_gain_db =
      run_phase(/*speech_level_dbfs=*/-60.0f, /*measure_ramp=*/true,
                &max_increase_step_db);
  // Phase 2: high speech level, the gain ramps down to unity.
  double max_decrease_step_db = 0.0;
  const double high_level_gain_db =
      run_phase(/*speech_level_dbfs=*/-10.0f, /*measure_ramp=*/true,
                &max_decrease_step_db);

  const double expected_max_step_db =
      config.max_gain_change_db_per_second * 0.01 + 0.01;

  const bool ceiling_ok = low_level_gain_db > noise_limited_gain_db - 1.0 &&
                          low_level_gain_db <= noise_limited_gain_db + 0.5;
  const bool unity_ok = high_level_gain_db < 0.5;
  const bool increase_rate_ok = max_increase_step_db <= expected_max_step_db;
  const bool decrease_rate_ok = max_decrease_step_db <= expected_max_step_db;

  std::cout << std::fixed << std::setprecision(2)
            << "  noise limited gain (ceiling " << noise_limited_gain_db
            << " dB) : " << (ceiling_ok ? "PASS" : "FAIL") << std::endl;
  std::cout << "  gain returns to unity        : " << (unity_ok ? "PASS" : "FAIL")
            << std::endl;
  std::cout << "  gain increase rate (" << max_increase_step_db
            << " dB/frame)   : " << (increase_rate_ok ? "PASS" : "FAIL")
            << std::endl;
  std::cout << "  gain decrease rate (" << max_decrease_step_db
            << " dB/frame)   : " << (decrease_rate_ok ? "PASS" : "FAIL")
            << std::endl;
  return ceiling_ok && unity_ok && increase_rate_ok && decrease_rate_ok;
}

// ---------------------------------------------------------------------------
// Scenario 4: internal voice activity detector (RNN VAD)
// ---------------------------------------------------------------------------

bool TestInternalVoiceActivityDetector() {
  std::cout << "--- Scenario 4: internal voice activity detector ---"
            << std::endl;

  const webrtc::AvailableCpuFeatures cpu_features = webrtc::GetAvailableCpuFeatures();
  std::cout << "  CPU features: " << cpu_features.ToString() << std::endl;

  webrtc::VoiceActivityDetectorWrapper vad(cpu_features, kSampleRateHz);
  webrtc::AudioBuffer buffer(kSampleRateHz, kChannels, kSampleRateHz, kChannels,
                             kSampleRateHz);

  constexpr size_t kFrames = 100;  // 1 s.

  const std::vector<float> silence = GenerateSilence(kFrames * kFrameSamples);
  double silence_probability = 0.0;
  for (size_t f = 0; f < kFrames; ++f) {
    WriteFrame(&buffer, silence.data() + f * kFrameSamples);
    silence_probability += vad.Analyze(buffer.view());
  }
  silence_probability /= kFrames;

  const std::vector<float> speech =
      GenerateSpeechLike(/*peak_amplitude=*/8192.0f, kFrames * kFrameSamples);
  double speech_probability = 0.0;
  for (size_t f = 0; f < kFrames; ++f) {
    WriteFrame(&buffer, speech.data() + f * kFrameSamples);
    speech_probability += vad.Analyze(buffer.view());
  }
  speech_probability /= kFrames;

  std::cout << std::fixed << std::setprecision(3)
            << "  mean speech probability (silence)     : " << silence_probability
            << std::endl;
  std::cout << "  mean speech probability (speech-like) : " << speech_probability
            << std::endl;

  const bool ok = silence_probability < 0.1 &&
                  speech_probability > silence_probability + 0.2;
  std::cout << "  speech detected over silence : " << (ok ? "PASS" : "FAIL")
            << std::endl;
  return ok;
}

}  // namespace

int main() {
  std::cout << "=== AGC2 Test ===" << std::endl;
  std::cout << "  Sample rate : " << kSampleRateHz << " Hz" << std::endl;
  std::cout << "  Channels    : " << kChannels << std::endl;
  std::cout << "  Frame size  : " << kFrameSamples << " samples (10 ms)"
            << std::endl;
  std::cout << std::endl;

  const bool fixed_gain_ok = TestFixedDigitalGainAndLimiter();
  std::cout << std::endl;
  const bool input_volume_ok = TestInputVolumeController();
  std::cout << std::endl;
  const bool adaptive_ok = TestAdaptiveDigitalGainController();
  std::cout << std::endl;
  const bool vad_ok = TestInternalVoiceActivityDetector();
  std::cout << std::endl;

  std::cout << "=== Summary ===" << std::endl;
  std::cout << "  fixed digital gain and limiter : "
            << (fixed_gain_ok ? "PASS" : "FAIL") << std::endl;
  std::cout << "  input volume controller        : "
            << (input_volume_ok ? "PASS" : "FAIL") << std::endl;
  std::cout << "  adaptive digital gain          : "
            << (adaptive_ok ? "PASS" : "FAIL") << std::endl;
  std::cout << "  internal voice activity        : " << (vad_ok ? "PASS" : "FAIL")
            << std::endl;

  const bool ok = fixed_gain_ok && input_volume_ok && adaptive_ok && vad_ok;
  return ok ? 0 : 1;
}