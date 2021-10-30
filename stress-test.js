const watcher = require('.');
const fs = require('fs-extra');
const path = require('path');

async function run() {
  console.log('Starting stress test');

  const dirs = new Array(10).fill('').map(() => {
    return path.join(
      fs.realpathSync(require('os').tmpdir()),
      Math.random().toString(31).slice(2),
    );
  });

  for (let dir of dirs) {
    fs.mkdirpSync(dir);
  }

  await new Promise((resolve) => setTimeout(resolve, 100));

  let subscriptions = [];
  for (let i = 0; i < 2500; i++) {
    console.log(`Doing something random ${i}`);

    const randomInt = Math.floor(Math.random() * 300);
    const dir = dirs[Math.floor(Math.random() * dirs.length)];

    if (randomInt < 100 && subscriptions.length) {
      subscriptions.pop().unsubscribe();
    }

    if (subscriptions.length < 25) {
      const subscription = await watcher.subscribe(dir, (err, events) => {
        // do nothing...
      });
      subscriptions.push(subscription);
    }

    fs.writeFile(
      path.join(dir, `file-${Math.round(Math.random() * 100000)}`),
      'test1',
    );
  }

  for (let s of subscriptions) {
    await s.unsubscribe();
  }

  await new Promise((resolve) => setTimeout(resolve, 500));

  console.log('Completed stress test');

  process.exit(0);
}

run();
