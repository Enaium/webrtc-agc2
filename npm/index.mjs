// Package entry point: binds the bundled WebAssembly module factory to the
// JavaScript wrapper in `agc2.mjs`.

import createAgc2Module from "./agc2_wasm.mjs";
import { createAgc2 as createAgc2WithModuleFactory } from "./agc2.mjs";

/**
 * Creates an AGC2 processor backed by the bundled WebAssembly module.
 *
 * @param {object} [options] See `createAgc2` in `agc2.mjs`.
 */
export function createAgc2(options) {
  return createAgc2WithModuleFactory(createAgc2Module, options);
}

export { createAgc2WithModuleFactory };