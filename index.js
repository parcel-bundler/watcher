const path = require('path');
const micromatch = require('micromatch');
const isGlob = require('is-glob');

let name = `@parcel/watcher-${process.platform}-${process.arch}`;
if (process.platform === 'linux') {
  const { MUSL, family } = require('detect-libc');
  if (family === MUSL) {
    name += '-musl';
  } else {
    name += '-glibc';
  }
}

let binding;
try {
  binding = require(name);
} catch (err) {
  try {
    binding = require('./build/Release/watcher.node');
  } catch (err) {
    try {
      binding = require('./build/Debug/watcher.node');
    } catch (err) {
      throw new Error(`No prebuild or local build of @parcel/watcher found. Tried ${name}. Please ensure it is installed (don't use --no-optional when installing with npm). Otherwise it is possible we don't support your platform yet. If this is the case, please report an issue to https://github.com/parcel-bundler/watcher.`);
    }
  }
}

function normalizeOptions(dir, opts = {}) {
  const { ignore, ...rest } = opts;

  if (Array.isArray(ignore)) {
    opts = { ...rest };

    for (const value of ignore) {
      if (isGlob(value)) {
        if (!opts.ignoreGlobs) {
          opts.ignoreGlobs = [];
        }

        const regex = micromatch.makeRe(value, {
          // We set `dot: true` to workaround an issue with the
          // regular expression on Linux where the resulting
          // negative lookahead `(?!(\\/|^)` was never matching
          // in some cases. See also https://bit.ly/3UZlQDm
          dot: true,
          // C++ does not support lookbehind regex patterns, they
          // were only added later to JavaScript engines
          // (https://bit.ly/3V7S6UL)
          lookbehinds: false
        });
        opts.ignoreGlobs.push(regex.source);
      } else {
        if (!opts.ignorePaths) {
          opts.ignorePaths = [];
        }

        opts.ignorePaths.push(path.resolve(dir, value));
      }
    }
  }

  return opts;
}

exports.writeSnapshot = (dir, snapshot, opts) => {
  return binding.writeSnapshot(
    path.resolve(dir),
    path.resolve(snapshot),
    normalizeOptions(dir, opts),
  );
};

exports.getEventsSince = (dir, snapshot, opts) => {
  return binding.getEventsSince(
    path.resolve(dir),
    path.resolve(snapshot),
    normalizeOptions(dir, opts),
  );
};

exports.subscribe = async (dir, fn, opts) => {
  dir = path.resolve(dir);
  opts = normalizeOptions(dir, opts);
  await binding.subscribe(dir, fn, opts);

  return {
    unsubscribe() {
      return binding.unsubscribe(dir, fn, opts);
    },
  };
};

exports.unsubscribe = (dir, fn, opts) => {
  return binding.unsubscribe(
    path.resolve(dir),
    fn,
    normalizeOptions(dir, opts),
  );
};
