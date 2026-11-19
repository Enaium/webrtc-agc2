# @enaium/webrtc-agc2

WebRTC AGC2 (Automatic Gain Control 2) compiled to WebAssembly: input volume
controller, adaptive digital gain, fixed digital gain and limiter, including
the RNN-based voice activity detector.

```bash
npm install @enaium/webrtc-agc2
```

```js
import { createAgc2 } from "@enaium/webrtc-agc2";

const agc2 = await createAgc2({
  sampleRateHz: 16000,
  numChannels: 1,
  enableAdaptiveDigital: true,
});

// One 10 ms frame of interleaved FloatS16 audio ([-32768, 32768]).
const frame = new Float32Array(agc2.frameSize * agc2.numChannels);
// ... fill `frame` from the capture device ...
agc2.process(frame);
// `frame` now holds the processed audio; AGC2 may also report a new input
// volume recommendation:
const recommendedVolume = agc2.recommendedInputVolume(); // number | null

agc2.setFixedGainDb(20);
agc2.free();
```

Use `createAgc2WithModuleFactory` when the wasm module must be instantiated
elsewhere, e.g. to share it between processors.

## Options

| Option | Default | Description |
|---|---|---|
| `sampleRateHz` | `16000` | Input sample rate; the frame size is `sampleRateHz / 100`. |
| `numChannels` | `1` | Number of interleaved channels per frame. |
| `enableInputVolumeController` | `false` | Enable input volume recommendations and clipping detection. |
| `enableAdaptiveDigital` | `false` | Enable the adaptive digital gain controller. |
| `fixedGainDb` | `0` | Fixed digital gain in dB, in `[0, 50)`. |
| `useInternalVad` | `true` | Use the built-in RNN VAD for the adaptive digital controller. |

## API

* `process(frame, { appliedInputVolume = 255, inputVolumeChanged = false })` —
  processes one 10 ms interleaved frame in place and returns it.
* `recommendedInputVolume()` — last input volume recommendation, or `null`.
* `setFixedGainDb(gainDb)` — sets the fixed digital gain.
* `setCaptureOutputUsed(used)` — informs AGC2 whether the capture output is
  used.
* `free()` — releases the wasm resources.

Audio uses the same FloatS16 convention as WebRTC's `AudioBuffer`.

## License

BSD 3-Clause. See `LICENSE`.