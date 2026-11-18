// Node test for the WebAssembly build of AGC2.
//
// Run with: pixi run -e wasm run-test-wasm
// (requires the wasm target to be built: .build-wasm/agc2_wasm.mjs)

import createAgc2Module from "../.build-wasm/agc2_wasm.mjs";
import { createAgc2 } from "../wasm/agc2.mjs";

const kSampleRateHz = 16000;
const kFrameSamples = 160;  // 10 ms at 16 kHz.
const kMaxAbsFloatS16 = 32768;

// ---------------------------------------------------------------------------
// Signal generation (FloatS16 scale: +/-32768 corresponds to 0 dBFS)
// ---------------------------------------------------------------------------

function generateSine(peakAmplitude, frequencyHz, numSamples) {
  const samples = new Float32Array(numSamples);
  for (let i = 0; i < numSamples; i++) {
    samples[i] = peakAmplitude *
        Math.sin(2 * Math.PI * frequencyHz * i / kSampleRateHz);
  }
  return samples;
}

function generateClippedSine(frequencyHz, numSamples) {
  const samples = generateSine(2 * kMaxAbsFloatS16, frequencyHz, numSamples);
  for (let i = 0; i < samples.length; i++) {
    samples[i] = Math.max(
        -kMaxAbsFloatS16, Math.min(samples[i], kMaxAbsFloatS16 - 1));
  }
  return samples;
}

function generateSpeechLike(peakAmplitude, numSamples) {
  const samples = new Float32Array(numSamples);
  const pitchHz = 120;
  const syllableRateHz = 3;
  const harmonics = [1.0, 0.7, 0.5, 0.4, 0.35, 0.3, 0.25, 0.2, 0.15];
  for (let i = 0; i < numSamples; i++) {
    const t = i / kSampleRateHz;
    let value = 0;
    for (let h = 0; h < harmonics.length; h++) {
      value += harmonics[h] * Math.sin(2 * Math.PI * pitchHz * (h + 1) * t);
    }
    const envelope = 0.5 * (1 - Math.cos(2 * Math.PI * syllableRateHz * t));
    samples[i] = value * envelope * peakAmplitude / 2.5;
  }
  return samples;
}

// ---------------------------------------------------------------------------
// Level measurement
// ---------------------------------------------------------------------------

function rmsDbfs(samples, start = 0, end = samples.length) {
  let sum = 0;
  for (let i = start; i < end; i++) {
    sum += samples[i] * samples[i];
  }
  const rms = Math.sqrt(sum / (end - start));
  return 20 * Math.log10(Math.max(rms, 1e-10) / kMaxAbsFloatS16);
}

function peakDbfs(samples) {
  let peak = 0;
  for (let i = 0; i < samples.length; i++) {
    peak = Math.max(peak, Math.abs(samples[i]));
  }
  return 20 * Math.log10(Math.max(peak, 1e-10) / kMaxAbsFloatS16);
}

// ---------------------------------------------------------------------------
// Frame helpers
// ---------------------------------------------------------------------------

function processSignal(agc2, input, onFrame) {
  const output = new Float32Array(input.length);
  const frame = new Float32Array(agc2.frameSize * agc2.numChannels);
  const numFrames = input.length / frame.length;
  for (let f = 0; f < numFrames; f++) {
    const offset = f * frame.length;
    frame.set(input.subarray(offset, offset + frame.length));
    agc2.process(frame);
    output.set(frame, offset);
    if (onFrame) {
      onFrame(f, frame);
    }
  }
  return output;
}

// ---------------------------------------------------------------------------
// Scenario 1: fixed digital gain and limiter
// ---------------------------------------------------------------------------

async function testFixedDigitalGainAndLimiter() {
  console.log("--- Scenario 1: fixed digital gain and limiter ---");

  const agc2 = await createAgc2(createAgc2Module, {
    sampleRateHz: kSampleRateHz,
    numChannels: 1,
    fixedGainDb: 0,
  });
  try {
    const framesPerPhase = 50;  // 0.5 s.
    const input = generateSine(16384, 1000, framesPerPhase * kFrameSamples);
    const inputRmsDbfs = rmsDbfs(input);
    const inputPeakDbfs = peakDbfs(input);

    const unityOutput = processSignal(agc2, input);
    const unityRmsDbfs = rmsDbfs(unityOutput);
    const unityPeakDbfs = peakDbfs(unityOutput);
    console.log(
        `  fixed gain   0 dB | input  : RMS ${inputRmsDbfs.toFixed(2)} dBFS,` +
        ` peak ${inputPeakDbfs.toFixed(2)} dBFS`);
    console.log(
        `  fixed gain   0 dB | output : RMS ${unityRmsDbfs.toFixed(2)} dBFS,` +
        ` peak ${unityPeakDbfs.toFixed(2)} dBFS`);

    const unityGainOk = unityPeakDbfs <= 0 &&
        Math.abs(unityRmsDbfs - inputRmsDbfs) < 0.5;

    agc2.setFixedGainDb(20);
    const boostedOutput = processSignal(agc2, input);
    const boostedRmsDbfs = rmsDbfs(boostedOutput);
    const boostedPeakDbfs = peakDbfs(boostedOutput);
    console.log(
        `  fixed gain  20 dB | output : RMS ${boostedRmsDbfs.toFixed(2)} dBFS,` +
        ` peak ${boostedPeakDbfs.toFixed(2)} dBFS`);

    const boostedOk = boostedPeakDbfs <= 0 &&
        boostedRmsDbfs - inputRmsDbfs > 3 && boostedPeakDbfs > -3;

    console.log(`  unity gain transparent : ${unityGainOk ? "PASS" : "FAIL"}`);
    console.log(
        `  +20 dB limited to 0 dBFS : ${boostedOk ? "PASS" : "FAIL"}`);
    return unityGainOk && boostedOk;
  } finally {
    agc2.free();
  }
}

// ---------------------------------------------------------------------------
// Scenario 2: input volume controller
// ---------------------------------------------------------------------------

async function testInputVolumeController() {
  console.log("--- Scenario 2: input volume controller ---");

  const initialVolume = 255;
  const clippedLevelStep = 15;
  const minInputVolume = 20;

  const agc2 = await createAgc2(createAgc2Module, {
    sampleRateHz: kSampleRateHz,
    numChannels: 1,
    enableInputVolumeController: true,
    // Only clipping modifies the volume in this scenario.
    useInternalVad: false,
  });
  try {
    const numFrames = 500;  // 5 s.
    const input = generateClippedSine(1000, numFrames * kFrameSamples);
    const frame = new Float32Array(kFrameSamples);

    let appliedVolume = initialVolume;
    let minVolume = appliedVolume;
    for (let f = 0; f < numFrames; f++) {
      frame.set(input.subarray(f * kFrameSamples, (f + 1) * kFrameSamples));
      agc2.process(frame, { appliedInputVolume: appliedVolume });
      const recommendation = agc2.recommendedInputVolume();
      if (recommendation !== null) {
        appliedVolume = recommendation;
      }
      minVolume = Math.min(minVolume, appliedVolume);
    }

    console.log(`  initial input volume      : ${initialVolume}`);
    console.log(`  final input volume        : ${appliedVolume}`);
    console.log(`  minimum input volume      : ${minVolume}`);

    const clippedOk = appliedVolume < initialVolume;
    const boundedOk = minVolume >= minInputVolume;
    const stepOk = (initialVolume - minVolume) % clippedLevelStep === 0;
    console.log(
        `  clipping lowers the volume : ${clippedOk ? "PASS" : "FAIL"}`);
    console.log(
        `  volume stays above minimum : ${boundedOk ? "PASS" : "FAIL"}`);
    console.log(`  volume lowered by steps    : ${stepOk ? "PASS" : "FAIL"}`);
    return clippedOk && boundedOk && stepOk;
  } finally {
    agc2.free();
  }
}

// ---------------------------------------------------------------------------
// Scenario 3: adaptive digital gain with the internal VAD
// ---------------------------------------------------------------------------

async function testAdaptiveDigitalGain() {
  console.log("--- Scenario 3: adaptive digital gain (internal VAD) ---");

  const agc2 = await createAgc2(createAgc2Module, {
    sampleRateHz: kSampleRateHz,
    numChannels: 1,
    enableAdaptiveDigital: true,
    useInternalVad: true,
  });
  try {
    const numFrames = 400;  // 4 s.
    const input = generateSpeechLike(8192, numFrames * kFrameSamples);
    const output = processSignal(agc2, input);

    const windowStart = 300 * kFrameSamples;
    const inputLevelDbfs = rmsDbfs(input, windowStart);
    const outputLevelDbfs = rmsDbfs(output, windowStart);
    const gainDb = outputLevelDbfs - inputLevelDbfs;
    console.log(
        `  input level  (last second) : ${inputLevelDbfs.toFixed(2)} dBFS`);
    console.log(
        `  output level (last second) : ${outputLevelDbfs.toFixed(2)} dBFS`);
    console.log(`  adaptive gain              : ${gainDb.toFixed(2)} dB`);

    const gainOk = gainDb > 3 && peakDbfs(output) <= 0;
    console.log(`  speech level is boosted    : ${gainOk ? "PASS" : "FAIL"}`);
    return gainOk;
  } finally {
    agc2.free();
  }
}

// ---------------------------------------------------------------------------
// Scenario 4: stereo pass-through (interleaving and multi-channel handling)
// ---------------------------------------------------------------------------

async function testStereoPassThrough() {
  console.log("--- Scenario 4: stereo pass-through ---");

  const agc2 = await createAgc2(createAgc2Module, {
    sampleRateHz: kSampleRateHz,
    numChannels: 2,
    fixedGainDb: 0,
  });
  try {
    const left = generateSine(8192, 1000, 50 * kFrameSamples);
    const input = new Float32Array(2 * left.length);
    for (let i = 0; i < left.length; i++) {
      input[2 * i] = left[i];  // Right channel stays silent.
    }

    const output = processSignal(agc2, input);
    const leftOut = new Float32Array(left.length);
    const rightOut = new Float32Array(left.length);
    for (let i = 0; i < left.length; i++) {
      leftOut[i] = output[2 * i];
      rightOut[i] = output[2 * i + 1];
    }

    const leftGainDb = rmsDbfs(leftOut) - rmsDbfs(left);
    let rightPeak = 0;
    for (let i = 0; i < rightOut.length; i++) {
      rightPeak = Math.max(rightPeak, Math.abs(rightOut[i]));
    }
    console.log(`  left channel gain  : ${leftGainDb.toFixed(3)} dB`);
    console.log(`  right channel peak : ${rightPeak}`);

    const ok = Math.abs(leftGainDb) < 0.01 && rightPeak === 0;
    console.log(`  channels stay separate : ${ok ? "PASS" : "FAIL"}`);
    return ok;
  } finally {
    agc2.free();
  }
}

// ---------------------------------------------------------------------------
// Scenario 5: silence stays silent
// ---------------------------------------------------------------------------

async function testSilenceIsTransparent() {
  console.log("--- Scenario 5: silence ---");

  const agc2 = await createAgc2(createAgc2Module, {
    sampleRateHz: kSampleRateHz,
    numChannels: 1,
    fixedGainDb: 0,
  });
  try {
    const input = new Float32Array(20 * kFrameSamples);  // All zeros.
    const output = processSignal(agc2, input);
    let peak = 0;
    for (let i = 0; i < output.length; i++) {
      peak = Math.max(peak, Math.abs(output[i]));
    }
    const ok = peak === 0;
    console.log(`  output stays silent : ${ok ? "PASS" : "FAIL"}`);
    return ok;
  } finally {
    agc2.free();
  }
}

// ---------------------------------------------------------------------------

async function main() {
  console.log("=== AGC2 WebAssembly Test ===");
  console.log(`  Sample rate : ${kSampleRateHz} Hz`);
  console.log(`  Frame size  : ${kFrameSamples} samples (10 ms)`);
  console.log();

  const results = [];
  results.push(await testFixedDigitalGainAndLimiter());
  console.log();
  results.push(await testInputVolumeController());
  console.log();
  results.push(await testAdaptiveDigitalGain());
  console.log();
  results.push(await testStereoPassThrough());
  console.log();
  results.push(await testSilenceIsTransparent());
  console.log();

  const names = [
    "fixed digital gain and limiter",
    "input volume controller",
    "adaptive digital gain",
    "stereo pass-through",
    "silence",
  ];
  console.log("=== Summary ===");
  for (let i = 0; i < results.length; i++) {
    console.log(`  ${names[i].padEnd(30)} : ${results[i] ? "PASS" : "FAIL"}`);
  }
  return results.every(Boolean);
}

process.exitCode = (await main()) ? 0 : 1;