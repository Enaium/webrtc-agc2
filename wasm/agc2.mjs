// Thin JavaScript wrapper around the AGC2 WebAssembly module.
//
// Usage (browser or Node, with the ES module build of the wasm target):
//
//   import createAgc2Module from "./.build-wasm/agc2_wasm.mjs";
//   import { createAgc2 } from "./wasm/agc2.mjs";
//
//   const agc2 = await createAgc2(createAgc2Module, {
//     sampleRateHz: 16000,
//     numChannels: 1,
//     enableAdaptiveDigital: true,
//   });
//   const frame = new Float32Array(agc2.frameSize * agc2.numChannels);
//   // ... fill `frame` with 10 ms of audio in the FloatS16 range ...
//   agc2.process(frame);
//   const volume = agc2.recommendedInputVolume(); // number | null
//   agc2.free();

/**
 * Creates an AGC2 processor.
 *
 * @param {function(): Promise<object>} moduleFactory Emscripten module factory
 *   (the default export of `agc2_wasm.mjs`).
 * @param {object} [options]
 * @param {number} [options.sampleRateHz=16000]
 * @param {number} [options.numChannels=1]
 * @param {boolean} [options.enableInputVolumeController=false]
 * @param {boolean} [options.enableAdaptiveDigital=false]
 * @param {number} [options.fixedGainDb=0] Fixed digital gain in dB, in [0, 50).
 * @param {boolean} [options.useInternalVad=true] Use the built-in RNN VAD for
 *   the adaptive digital controller.
 */
export async function createAgc2(moduleFactory, options = {}) {
  const {
    sampleRateHz = 16000,
    numChannels = 1,
    enableInputVolumeController = false,
    enableAdaptiveDigital = false,
    fixedGainDb = 0,
    useInternalVad = true,
  } = options;

  const module = await moduleFactory();
  const frameSize = module._agc2_frame_size(sampleRateHz);
  if (frameSize <= 0) {
    throw new Error(`Unsupported sample rate: ${sampleRateHz}`);
  }
  const handle = module._agc2_create(
      sampleRateHz, numChannels, enableInputVolumeController ? 1 : 0,
      enableAdaptiveDigital ? 1 : 0, fixedGainDb, useInternalVad ? 1 : 0);
  if (!handle) {
    throw new Error("agc2_create failed (invalid configuration)");
  }

  const frameLength = frameSize * numChannels;
  const pointer = module._malloc(frameLength * Float32Array.BYTES_PER_ELEMENT);

  return {
    sampleRateHz,
    numChannels,
    frameSize,

    /**
     * Processes one 10 ms frame of interleaved audio in place.
     *
     * Samples use the FloatS16 scale ([-32768, 32768]), like WebRTC's
     * `AudioBuffer`. `frame` must hold `frameSize * numChannels` samples.
     *
     * @param {Float32Array} frame
     * @param {object} [frameOptions]
     * @param {number} [frameOptions.appliedInputVolume=255] Input volume
     *   currently applied by the capture device (0-255); only used when the
     *   input volume controller is enabled.
     * @param {boolean} [frameOptions.inputVolumeChanged=false]
     * @returns {Float32Array} `frame`, with the processed audio.
     */
    process(frame, frameOptions = {}) {
      const {
        appliedInputVolume = 255,
        inputVolumeChanged = false,
      } = frameOptions;
      if (frame.length !== frameLength) {
        throw new Error(
            `Expected a frame of ${frameLength} samples, got ${frame.length}`);
      }
      module.HEAPF32.set(frame, pointer >> 2);
      module._agc2_process_frame(
          handle, pointer, appliedInputVolume, inputVolumeChanged ? 1 : 0);
      frame.set(module.HEAPF32.subarray(
          pointer >> 2, (pointer >> 2) + frameLength));
      return frame;
    },

    /** Returns the recommended input volume, or null if there is none. */
    recommendedInputVolume() {
      const volume = module._agc2_recommended_input_volume(handle);
      return volume < 0 ? null : volume;
    },

    /** Sets the fixed digital gain in dB. */
    setFixedGainDb(gainDb) {
      module._agc2_set_fixed_gain_db(handle, gainDb);
    },

    /** Informs AGC2 whether the capture output is used. */
    setCaptureOutputUsed(captureOutputUsed) {
      module._agc2_set_capture_output_used(handle, captureOutputUsed ? 1 : 0);
    },

    /** Releases the wasm resources. */
    free() {
      module._free(pointer);
      module._agc2_destroy(handle);
    },
  };
}