const exec = require('child_process').execSync;
const fs = require('fs');
const pkg = require('../package.json');

const dir = `${__dirname}/..`;
try {
  fs.mkdirSync(dir + '/npm/wasm');
} catch (err) { }

let dts = fs.readFileSync(`${dir}/index.d.ts`, 'utf8');
dts += `
/** Initializes the web assembly module. */
export default function init(input?: string | URL | Request): Promise<void>;
`;
fs.writeFileSync(`${dir}/npm/wasm/index.d.ts`, dts);

let readme = fs.readFileSync(`${dir}/README.md`, 'utf8');
readme = readme.replace('# ⚡️ Lightning CSS', '# ⚡️ lightningcss-wasm');
fs.writeFileSync(`${dir}/npm/wasm/README.md`, readme);

let js = fs.readFileSync(`${dir}/wasm/index.mjs`, 'utf8');
js = js.replace('../build/Debug/watcher.wasm', 'watcher.wasm');
js = js.replace('../wrapper.js', './wrapper.js');
fs.writeFileSync(`${dir}/npm/wasm/index.mjs`, js);

fs.copyFileSync(`${dir}/wrapper.js`, `${dir}/npm/wasm/wrapper.js`);
fs.copyFileSync(`${dir}/wasm/watcher.wasm`, `${dir}/npm/wasm/watcher.wasm`);
fs.cpSync(`${dir}/node_modules/napi-wasm`, `${dir}/npm/wasm/node_modules/napi-wasm`, {recursive: true});

let wasmPkg = { ...pkg };
wasmPkg.name = '@parcel/watcher-wasm';
wasmPkg.main = 'index.mjs';
wasmPkg.module = 'index.mjs';
wasmPkg.types = 'index.d.ts';
wasmPkg.sideEffects = false;
wasmPkg.files = ['*.js', '*.mjs', '*.d.ts', '*.wasm'];
wasmPkg.dependencies = {
  'napi-wasm': pkg.devDependencies['napi-wasm'],
  'is-glob': pkg.dependencies['is-glob'],
  'micromatch': pkg.dependencies['micromatch']
};
wasmPkg.bundledDependencies = ['napi-wasm']; // for stackblitz
delete wasmPkg.exports;
delete wasmPkg.binary;
delete wasmPkg['lint-staged'];
delete wasmPkg.husky;
delete wasmPkg.devDependencies;
delete wasmPkg.optionalDependencies;
delete wasmPkg.targets;
delete wasmPkg.scripts;
fs.writeFileSync(`${dir}/npm/wasm/package.json`, JSON.stringify(wasmPkg, false, 2) + '\n');
