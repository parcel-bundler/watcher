const fschanges = require('./');
const fs = require('fs');

let dir = process.cwd();

async function run() {
  console.time('read');
  let changes = await fschanges.getEventsSince(dir, dir + '/token.txt', {
    // backend: 'brute-force',
    ignore: [dir + '/.git']
  });
  console.timeEnd('read');
  console.log(changes);

  console.time('write');
  await fschanges.writeSnapshot(dir, dir + '/token.txt', {
    // backend: 'brute-force',
    ignore: [dir + '/.git']
  });
  console.timeEnd('write');
}

// run();

let fn = events => {
  console.log(events);
  // fschanges.unsubscribe(dir, fn, {ignore: [dir + '/.git']});
};

fschanges.subscribe(dir, fn, {ignore: [dir + '/.git']});

// let w = new Watcher(dir);
// w.getEventsSince(snapshotPath);
// w.writeSnapshot(snapshotPath);
// w.subscribe(events => {

// });
// w.unsubscribe();
