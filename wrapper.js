const path = require('path');
const globToRegExp = require('glob-to-regexp');
const isGlob = require('is-glob');

// Patterns containing extglob syntax that glob-to-regexp doesn't support
const NEEDS_PICOMATCH = /[{(|)}\[\]!@+]/;
let picomatch;

function globToRegex(value) {
  if (NEEDS_PICOMATCH.test(value)) {
    if (!picomatch) {
      picomatch = require('picomatch');
    }
    return picomatch.makeRe(value, {
      dot: true,
      windows: process.platform === 'win32',
    });
  }
  return globToRegExp(value, { globstar: true });
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

        const regex = globToRegex(value);
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

exports.createWrapper = (binding) => {
  return {
    writeSnapshot(dir, snapshot, opts) {
      return binding.writeSnapshot(
        path.resolve(dir),
        path.resolve(snapshot),
        normalizeOptions(dir, opts),
      );
    },
    getEventsSince(dir, snapshot, opts) {
      return binding.getEventsSince(
        path.resolve(dir),
        path.resolve(snapshot),
        normalizeOptions(dir, opts),
      );
    },
    async subscribe(dir, fn, opts) {
      dir = path.resolve(dir);
      opts = normalizeOptions(dir, opts);
      await binding.subscribe(dir, fn, opts);

      return {
        unsubscribe() {
          return binding.unsubscribe(dir, fn, opts);
        },
      };
    },
    unsubscribe(dir, fn, opts) {
      return binding.unsubscribe(
        path.resolve(dir),
        fn,
        normalizeOptions(dir, opts),
      );
    }
  };
};
