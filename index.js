const binding = require('bindings')('watcher.node');
const path = require('path');

function normalizeOptions(dir, opts = {}) {
  if (Array.isArray(opts.ignore)) {
    opts = Object.assign({}, opts, {
      ignore: opts.ignore.map(ignore => path.resolve(dir, ignore))
    });
  }

  return opts;
}

exports.writeSnapshot = (dir, snapshot, opts) => {
  return binding.writeSnapshot(path.resolve(dir), path.resolve(snapshot), normalizeOptions(dir, opts));
};

exports.getEventsSince = (dir, snapshot, opts) => {
  return binding.getEventsSince(path.resolve(dir), path.resolve(snapshot), normalizeOptions(dir, opts));
};

exports.subscribe = async (dir, fn, opts) => {
  dir = path.resolve(dir);
  opts = normalizeOptions(dir, opts);
  await binding.subscribe(dir, fn, opts);

  return {
    unsubscribe() {
      return binding.unsubscribe(dir, fn, opts);
    }
  };
};

exports.unsubscribe = (dir, fn, opts) => {
  return binding.unsubscribe(path.resolve(dir), fn, normalizeOptions(dir, opts));
};

exports.getTree = (dir, opts) => {
  return binding.getTree(path.resolve(dir), normalizeOptions(dir, opts));
};
