const fs = require('fs');
const path = require('path');
const picomatch = require('picomatch');
const isGlob = require('is-glob');

// The backends report canonical paths, and every ignore check is made against
// the watched directory: `ignorePaths` are resolved from it, and `ignoreGlobs`
// are matched against the event path with that directory stripped off. So a
// caller who passes a non-canonical directory gets no ignoring at all, silently
// -- on macOS `os.tmpdir()` is `/var/...` while events arrive under the
// `/private/var` firmlink, and the prefix comparison fails before any pattern is
// consulted. Canonicalize once here so both sides agree.
//
// Falls back to the plain resolve when the path cannot be canonicalized, which
// keeps a not-yet-created directory behaving as it did before.
function resolveDir(dir) {
  const resolved = path.resolve(dir);
  try {
    return fs.realpathSync(resolved);
  } catch (err) {
    return resolved;
  }
}

function normalizeOptions(dir, opts = {}) {
  const {ignore, ...rest} = opts;

  if (Array.isArray(ignore)) {
    opts = {...rest};

    for (const value of ignore) {
      if (value instanceof RegExp) {
        if (value.flags !== '') {
          throw new Error(
            `RegExp ignore patterns must not have flags (got /${value.source}/${value.flags}). Flags are not supported by the native matcher.`,
          );
        }
        if (!opts.ignoreGlobs) {
          opts.ignoreGlobs = [];
        }
        // The native backend uses std::regex_match (full-string match), but
        // callers expect JS .test() semantics (substring search). Wrapping the
        // source in ^[\\s\\S]*(?:…)[\\s\\S]*$ achieves that. The (?:…) group
        // isolates the source so leading/trailing | in the source stays contained.
        opts.ignoreGlobs.push(`^[\\s\\S]*(?:${value.source})[\\s\\S]*$`);
      } else if (isGlob(value)) {
        if (!opts.ignoreGlobs) {
          opts.ignoreGlobs = [];
        }

        const regex = picomatch.makeRe(value, {
          // We set `dot: true` to workaround an issue with the
          // regular expression on Linux where the resulting
          // negative lookahead `(?!(\\/|^)` was never matching
          // in some cases. See also https://bit.ly/3UZlQDm
          dot: true,
          windows: process.platform === 'win32',
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

exports.createWrapper = (binding) => {
  return {
    writeSnapshot(dir, snapshot, opts) {
      dir = resolveDir(dir);
      return binding.writeSnapshot(
        dir,
        path.resolve(snapshot),
        normalizeOptions(dir, opts),
      );
    },
    getEventsSince(dir, snapshot, opts) {
      dir = resolveDir(dir);
      return binding.getEventsSince(
        dir,
        path.resolve(snapshot),
        normalizeOptions(dir, opts),
      );
    },
    async subscribe(dir, fn, opts) {
      dir = resolveDir(dir);
      opts = normalizeOptions(dir, opts);
      await binding.subscribe(dir, fn, opts);

      return {
        unsubscribe() {
          return binding.unsubscribe(dir, fn, opts);
        },
      };
    },
    unsubscribe(dir, fn, opts) {
      dir = resolveDir(dir);
      return binding.unsubscribe(dir, fn, normalizeOptions(dir, opts));
    },
  };
};
