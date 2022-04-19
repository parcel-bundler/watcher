const binding = require('node-gyp-build')(__dirname);
const path = require('path');
const micromatch = require('micromatch');
const isGlob = require('is-glob');

function normalizeOptions(dir, opts = {}) {
  const { ignore, ...rest } = opts;

  if (Array.isArray(ignore)) {
    opts = { ...rest };

    for (const value of ignore) {
      if (isGlob(value)) {
        if (!opts.ignoreGlobs) {
          opts.ignoreGlobs = [];
        }

        opts.ignoreGlobs.push(micromatch.makeRe(value).toString());
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
