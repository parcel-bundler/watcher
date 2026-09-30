const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const watcher = require('../..');

async function releaseCallback(root) {
  const callback = () => {};
  const reference = new WeakRef(callback);
  const subscription = await watcher.subscribe(root, callback);
  await subscription.unsubscribe();
  return reference;
}

async function main() {
  const root = await fs.promises.mkdtemp(
    path.join(fs.realpathSync(os.tmpdir()), 'parcel-callback-release-'),
  );
  // Keep one subscription alive while exercising shared watcher cleanup.
  const anchor = await watcher.subscribe(root, () => {});
  try {
    const references = [];
    for (let i = 0; i < 64; i++) references.push(await releaseCallback(root));
    const deadline = Date.now() + 10000;
    let retained = true;
    while (retained && Date.now() < deadline) {
      // WeakRef.deref() retains its target for the current turn.
      await new Promise(setImmediate);
      global.gc();
      await new Promise(setImmediate);
      retained = references.some(
        (reference) => reference.deref() !== undefined,
      );
    }
    assert(
      !retained,
      'Unsubscribed callbacks must not retain their JS closures',
    );
  } finally {
    await anchor.unsubscribe();
    await fs.promises.rmdir(root);
  }
  console.log('callbacks-released');
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
