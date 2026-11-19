// Smoke test for the staged npm package: imports the package entry point
// (`.build-npm/index.mjs`) and checks that the bundled wasm module applies the
// fixed digital gain and limiter.

import { createAgc2 } from "../.build-npm/index.mjs";

const kSampleRateHz = 16000;
const kMaxAbsFloatS16 = 32768;

function rmsDbfs(samples) {
  let sum = 0;
  for (const sample of samples) {
    sum += sample * sample;
  }
  const rms = Math.sqrt(sum / samples.length);
  return 20 * Math.log10(Math.max(rms, 1e-10) / kMaxAbsFloatS16);
}

function peakDbfs(samples) {
  let peak = 0;
  for (const sample of samples) {
    peak = Math.max(peak, Math.abs(sample));
  }
  return 20 * Math.log10(Math.max(peak, 1e-10) / kMaxAbsFloatS16);
}

function sine(peakAmplitude, frequencyHz, numSamples) {
  const samples = new Float32Array(numSamples);
  for (let i = 0; i < numSamples; i++) {
    samples[i] = peakAmplitude *
        Math.sin(2 * Math.PI * frequencyHz * i / kSampleRateHz);
  }
  return samples;
}

function processFrames(agc2, input) {
  const output = new Float32Array(input.length);
  const frame = new Float32Array(agc2.frameSize * agc2.numChannels);
  for (let offset = 0; offset < input.length; offset += frame.length) {
    frame.set(input.subarray(offset, offset + frame.length));
    agc2.process(frame);
    output.set(frame, offset);
  }
  return output;
}

const agc2 = await createAgc2({sampleRateHz: kSampleRateHz, numChannels: 1});
try {
  const input = sine(16384, 1000, 50 * agc2.frameSize);
  const inputRmsDbfs = rmsDbfs(input);

  const unityOutput = processFrames(agc2, input);
  const unityOk = peakDbfs(unityOutput) <= 0 &&
      Math.abs(rmsDbfs(unityOutput) - inputRmsDbfs) < 0.5;

  agc2.setFixedGainDb(20);
  const boostedOutput = processFrames(agc2, input);
  const boostOk = peakDbfs(boostedOutput) <= 0 &&
      rmsDbfs(boostedOutput) - inputRmsDbfs > 3;

  console.log(
      `  ${agc2.frameSize} samples/frame ${agc2.numChannels} channel(s), ` +
      `input RMS ${inputRmsDbfs.toFixed(2)} dBFS`);
  console.log(`  unity gain transparent : ${unityOk ? "PASS" : "FAIL"}`);
  console.log(`  +20 dB limited to 0 dBFS : ${boostOk ? "PASS" : "FAIL"}`);
  process.exitCode = unityOk && boostOk ? 0 : 1;
} finally {
  agc2.free();
}