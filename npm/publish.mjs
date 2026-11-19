// Publishes the staged npm package in `.build-npm/`.
//
// The token is read from the environment (`NPM_PUBLISH_KEY`, or
// `NODE_AUTH_TOKEN`) and written to a `.npmrc` file inside the staging
// directory, where npm expands it at publish time; `.npmrc` is never included
// in the published tarball.
//
// Usage: node npm/publish.mjs [--tag <dist-tag>] [--dry-run]

import { spawnSync } from "node:child_process";
import { existsSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const repoRoot = dirname(dirname(fileURLToPath(import.meta.url)));
const stagingDir = join(repoRoot, ".build-npm");

if (!existsSync(join(stagingDir, "package.json"))) {
  console.error("No staged package: run `pixi run -e wasm pack-npm` first.");
  process.exit(1);
}

const args = process.argv.slice(2);
const dryRun = args.includes("--dry-run");
const tagIndex = args.indexOf("--tag");
const tag = tagIndex >= 0 ? args[tagIndex + 1] : "latest";

const token = process.env.NODE_AUTH_TOKEN ?? process.env.NPM_PUBLISH_KEY;
if (!token) {
  console.error(
      "npm token is not set: export NPM_PUBLISH_KEY=<token> " +
      "(or NODE_AUTH_TOKEN).");
  process.exit(1);
}

// `${NODE_AUTH_TOKEN}` is expanded by npm from the environment, so the token
// never lands in the package metadata.
writeFileSync(
    join(stagingDir, ".npmrc"),
    "//registry.npmjs.org/:_authToken=${NODE_AUTH_TOKEN}\n");

const result = spawnSync(
    "npm",
    ["publish", "--access", "public", "--tag", tag, ...(dryRun ? ["--dry-run"] : [])],
    {cwd: stagingDir, stdio: "inherit", env: {...process.env, NODE_AUTH_TOKEN: token}});

process.exit(result.status ?? 1);