# WebRTC AGC2

A standalone extraction of the WebRTC Automatic Gain Control 2 (AGC2) module.

Extracted from `webrtc/src` at commit `3406fde1d1` (2026-11-17), following the
layout of the sibling [AEC3](../aec3) extraction.

## Contents

* `GainController2` (`audio_processing/gain_controller2.h`) — the AGC2 entry
  point: input volume controller, adaptive digital controller, fixed digital
  controller and limiter.
* The AGC2 sub-modules (`audio_processing/agc2/`), including the RNN-based
  voice activity detector (`audio_processing/agc2/rnn_vad/`).
* The APM glue the module needs: `AudioBuffer`, `ChannelBuffer`, the capture
  mixer, the splitting filter, the resampler, `ApmDataDumper` and the field
  trial parser (`rtc_base/experiments/field_trial_parser`).

## Build

This project uses [pixi](https://pixi.sh) with CMake and Ninja.

```bash
pixi run -e build configure
pixi run -e build build
```

This produces `libwebrtc_agc2.dylib` (macOS), `libwebrtc_agc2.so` (Linux), or
`webrtc_agc2.dll` (Windows) in `.build/`.

pixi provides CMake and Ninja; the compiler is taken from the platform
toolchain (AppleClang, GCC/Clang, MSVC).

## Test

```bash
pixi run -e build configure-test
pixi run -e build build-test
pixi run -e build run-test
```

The test synthesizes its input signals and exercises four scenarios:

1. Fixed digital gain and limiter through `GainController2` (unity gain is
   transparent, +20 dB is limited to 0 dBFS).
2. Input volume controller through `GainController2` (sustained clipping lowers
   the recommended input volume step by step, bounded by `min_input_volume`).
3. `AdaptiveDigitalGainController` directly (gain ramps up at
   `max_gain_change_db_per_second`, stops at the noise-limited ceiling, and
   ramps back down when the speech level rises).
4. `VoiceActivityDetectorWrapper` (the RNN VAD weights are loaded and inference
   separates speech-like audio from silence).

## Usage

```cpp
#include "api/audio_processing.h"
#include "api/environment.h"
#include "audio_processing/agc2/input_volume_controller.h"
#include "audio_processing/audio_buffer.h"
#include "audio_processing/gain_controller2.h"

webrtc::Environment env;

webrtc::AudioProcessing::Config::GainController2 config;
config.enabled = true;
config.adaptive_digital.enabled = true;

webrtc::GainController2 agc2(env, config, webrtc::InputVolumeController::Config{},
                             /*sample_rate_hz=*/16000, /*num_channels=*/1,
                             /*use_internal_vad=*/true);

webrtc::AudioBuffer buffer(16000, 1, 16000, 1, 16000, 1);
// Fill `buffer` with 10 ms of capture audio (FloatS16 range), then:
agc2.Analyze(/*applied_input_volume=*/255, buffer);
agc2.Process(/*input_volume_changed=*/false, &buffer);
// The processed audio is in `buffer`; `agc2.recommended_input_volume()` holds
// the input volume recommendation (when the input volume controller is on).
```

Audio is exchanged in 10 ms frames at a FloatS16 scale (values in
`[-32768, 32768]`), like WebRTC's `AudioBuffer`.

## Extraction notes

Layout follows the AEC3 extraction: `modules/audio_processing/...` becomes
`audio_processing/...`, `api/audio/...` and `api/environment/...` become
`api/...`. All other include paths are unchanged, and `src/` is the only
include root.

Deviations from the WebRTC sources:

* `api/audio_processing.h` and `api/audio_frame.h` are trimmed copies: they
  keep the parts AGC2 depends on (`AudioProcessing::Config`, `StreamConfig`,
  `SampleRateToDefaultChannelSize`) and drop the rest of the APM interface.
* `api/environment.h` is a standalone `Environment` (no-op field trials) instead
  of the APM builder environment.
* `absl/` contains minimal stubs for the Abseil facilities used by the
  extracted code (`string_view`, `StrCat`, `HasAbslStringify`, `AnyInvocable`,
  `NoDestructor`, `c_fill`, `StartsWith`/`EndsWith`, `absl_nonnull`/
  `absl_nullable`).
* `third_party/pffft` and `third_party/rnnoise` are not part of the WebRTC git
  repository; they are vendored from Chromium revision `08c193172d` (the
  revision pinned by WebRTC's `DEPS`).
* Test-only files (`*_unittest.cc`, `agc2_testing_common.*`,
  `compute_interpolated_gain_curve.*`, `limiter_db_gain_curve.*`,
  `vector_float_frame.*`, `rnn_vad/test_utils.*`, `rnn_vad_tool.cc`) are not
  part of the extraction; unused includes of the files that remain were
  dropped.
* `audio_processing/logging/apm_data_dumper.{h,cc}` include `build_config.h`,
  which defines `WEBRTC_APM_DEBUG_DUMP` (set to `0`).

The build defines `WEBRTC_MAC`/`WEBRTC_POSIX` (or `WEBRTC_LINUX`/`WEBRTC_WIN`),
`WEBRTC_HAS_NEON` on ARM, and links CoreFoundation on macOS for the logging
back-end.

## License

BSD 3-Clause (same as WebRTC). See [LICENSE](LICENSE).