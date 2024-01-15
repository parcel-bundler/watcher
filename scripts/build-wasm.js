const esbuild = require('esbuild');
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
fs.writeFileSync(`${dir}/npm/wasm/README.md`, readme);

let js = fs.readFileSync(`${dir}/wasm/index.mjs`, 'utf8');
js = js.replace('../build/Debug/watcher.wasm', 'watcher.wasm');
js = js.replace('../wrapper.js', './wrapper.js');
fs.writeFileSync(`${dir}/npm/wasm/index.mjs`, js);

fs.copyFileSync(`${dir}/wrapper.js`, `${dir}/npm/wasm/wrapper.js`);
fs.copyFileSync(`${dir}/wasm/watcher.wasm`, `${dir}/npm/wasm/watcher.wasm`);
fs.cpSync(`${dir}/node_modules/napi-wasm`, `${dir}/npm/wasm/node_modules/napi-wasm`, {recursive: true});

const cjsBuild = {
  entryPoints: [`${dir}/npm/wasm/index.mjs`],
  bundle: true,
  format: 'cjs',
  platform: 'node',
  packages: 'external',
  outdir: `${dir}/npm/wasm`,
  outExtension: { '.js': '.cjs' },
  inject: [`${dir}/wasm/import.meta.url-polyfill.js`],
  define: { 'import.meta.url': 'import_meta_url' },
};
esbuild.build(cjsBuild).catch(console.error);

const wasmPkg = { ...pkg };
wasmPkg.name = '@parcel/watcher-wasm';
wasmPkg.main = 'index.mjs';
wasmPkg.module = 'index.mjs';
wasmPkg.types = 'index.d.ts';
wasmPkg.sideEffects = false;
wasmPkg.files = ['*.js', '*.cjs', '*.mjs', '*.d.ts', '*.wasm'];
wasmPkg.dependencies = {
  'napi-wasm': pkg.devDependencies['napi-wasm'],
  'is-glob': pkg.dependencies['is-glob'],
  'micromatch': pkg.dependencies['micromatch']
};
wasmPkg.exports = {
  types: './index.d.ts',
  import: './index.mjs',
  require: './index.cjs'
};
wasmPkg.bundledDependencies = ['napi-wasm']; // for stackblitz
delete wasmPkg.binary;
delete wasmPkg['lint-staged'];
delete wasmPkg.husky;
delete wasmPkg.devDependencies;
delete wasmPkg.optionalDependencies;
delete wasmPkg.targets;
delete wasmPkg.scripts;
fs.writeFileSync(`${dir}/npm/wasm/package.json`, JSON.stringify(wasmPkg, false, 2) + '\n');
