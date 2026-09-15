/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_AUDIO_FRAME_H_
#define API_AUDIO_AUDIO_FRAME_H_

#include <stddef.h>

#include "rtc_base/checks.h"

namespace webrtc {

// Default webrtc buffer size in milliseconds.
constexpr size_t kDefaultAudioBufferLengthMs = 10u;

// Default total number of audio buffers per second based on the default length.
constexpr size_t kDefaultAudioBuffersPerSec =
    1000u / kDefaultAudioBufferLengthMs;

// Returns the number of samples a buffer needs to hold for ~10ms of a single
// audio channel at a given sample rate.
// See also `AudioProcessing::GetFrameSize()`.
inline size_t SampleRateToDefaultChannelSize(size_t sample_rate) {
  // Basic sanity check. 192kHz is the highest supported input sample rate.
  RTC_DCHECK_LE(sample_rate, 192000);
  return sample_rate / kDefaultAudioBuffersPerSec;
}

}  // namespace webrtc

#endif  // API_AUDIO_AUDIO_FRAME_H_