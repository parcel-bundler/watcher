const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const watcher = require('../..');

async function main() {
  const root = await fs.promises.mkdtemp(
    path.join(fs.realpathSync(os.tmpdir()), 'parcel-failed-subscription-'),
  );
  let subscription;
  try {
    for (let i = 0; i < 128; i++) {
      await Promise.all([
        assert.rejects(
          watcher.subscribe(path.join(root, `missing-${i}`), () => {}, {
            backend: 'fs-events',
          }),
        ),
        ...Array.from({length: 1024}, () => fs.promises.lstat(root)),
      ]);
      global.gc();
    }

    const file = path.join(root, 'still-watching.txt');
    let received;
    const changed = new Promise((resolve, reject) => {
      received = (error, events) => {
        if (error) reject(error);
        else if (events.some((event) => event.path === file)) resolve();
      };
    });
    subscription = await watcher.subscribe(root, received, {
      backend: 'fs-events',
    });
    await fs.promises.writeFile(file, 'still alive');
    await changed;
    await fs.promises.unlink(file);
  } finally {
    if (subscription) await subscription.unsubscribe();
    await fs.promises.rmdir(root);
  }
  console.log('failed-subscriptions-survived');
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
