// Stages the publishable npm package in `.build-npm/` from the wasm build
// artifacts, the JS wrapper and the package metadata in `npm/`.

import { cpSync, existsSync, mkdirSync, readFileSync, rmSync, statSync }
  from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const repoRoot = dirname(dirname(fileURLToPath(import.meta.url)));
const stagingDir = join(repoRoot, ".build-npm");

const files = [
  ["npm/package.json", "package.json"],
  ["npm/index.mjs", "index.mjs"],
  ["npm/README.md", "README.md"],
  ["wasm/agc2.mjs", "agc2.mjs"],
  [".build-wasm/agc2_wasm.mjs", "agc2_wasm.mjs"],
  [".build-wasm/agc2_wasm.wasm", "agc2_wasm.wasm"],
  ["LICENSE", "LICENSE"],
];

// Fail on the first missing input so that the error names the missing step.
for (const [source] of files) {
  if (!existsSync(join(repoRoot, source))) {
    console.error(
        `Missing ${source}. Run \`pixi run -e wasm build-wasm\` first.`);
    process.exit(1);
  }
}

rmSync(stagingDir, {recursive: true, force: true});
mkdirSync(stagingDir, {recursive: true});
for (const [source, target] of files) {
  cpSync(join(repoRoot, source), join(stagingDir, target));
}

const {name, version} =
    JSON.parse(readFileSync(join(stagingDir, "package.json"), "utf8"));
console.log(`Staged ${name}@${version} in .build-npm/`);
for (const [, target] of files) {
  const size = statSync(join(stagingDir, target)).size;
  console.log(`  ${target.padEnd(18)} ${size} bytes`);
}