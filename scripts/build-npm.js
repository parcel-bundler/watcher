const fs = require('fs');
const pkg = require('../package.json');

const dir = `${__dirname}/..`;
const triples = [
  {
    platform: 'darwin',
    arch: 'x64'
  },
  {
    platform: 'darwin',
    arch: 'arm64'
  },
  {
    platform: 'win32',
    arch: 'x64'
  },
  {
    platform: 'win32',
    arch: 'arm64'
  },
  {
    platform: 'linux',
    arch: 'x64',
    libc: 'glibc'
  },
  {
    platform: 'linux',
    arch: 'x64',
    libc: 'musl'
  },
  {
    platform: 'linux',
    arch: 'arm64',
    libc: 'glibc'
  },
  {
    platform: 'linux',
    arch: 'arm64',
    libc: 'musl'
  },
  {
    platform: 'linux',
    arch: 'arm',
    libc: 'glibc'
  },
  {
    platform: 'android',
    arch: 'arm64'
  },
  {
    platform: 'freebsd',
    arch: 'x64'
  }
];

let optionalDependencies = {};

try {
  fs.mkdirSync(dir + '/npm');
} catch (err) { }

for (let triple of triples) {
  // Add the libc field to package.json to avoid downloading both
  // `gnu` and `musl` packages in Linux.
  let {platform, arch, libc} = triple;
  let t = `${platform}-${arch}`;
  if (libc) {
    t += '-' + libc;
  }

  buildNode(triple, t);
}

pkg.optionalDependencies = optionalDependencies;
fs.writeFileSync(`${dir}/package.json`, JSON.stringify(pkg, false, 2) + '\n');

function buildNode(triple, t) {
  let pkg2 = { ...pkg };
  pkg2.name = `@parcel/watcher-${t}`;
  pkg2.os = [triple.platform];
  pkg2.cpu = [triple.arch];
  if (triple.libc) {
    pkg2.libc = [triple.libc];
  }
  pkg2.main = 'watcher.node';
  pkg2.files = ['watcher.node'];
  delete pkg2.binary;
  delete pkg2.devDependencies;
  delete pkg2.dependencies;
  delete pkg2.optionalDependencies;
  delete pkg2.targets;
  delete pkg2.scripts;
  delete pkg2.types;
  delete pkg2['lint-staged'];
  delete pkg2.husky;

  optionalDependencies[pkg2.name] = pkg.version;

  try {
    fs.mkdirSync(dir + '/npm/' + t);
  } catch (err) { }
  fs.writeFileSync(`${dir}/npm/${t}/package.json`, JSON.stringify(pkg2, false, 2) + '\n');
  fs.copyFileSync(`${dir}/prebuilds/${triple.platform}-${triple.arch}/node.napi.${triple.libc || 'glibc'}.node`, `${dir}/npm/${t}/watcher.node`);
  fs.writeFileSync(`${dir}/npm/${t}/README.md`, `This is the ${t} build of @parcel/watcher. See https://github.com/parcel-bundler/watcher for details.`);
  fs.copyFileSync(`${dir}/LICENSE`, `${dir}/npm/${t}/LICENSE`);
}
