const fschanges = require('./');
const fs = require('fs');

let dir = process.cwd();

async function run() {
  console.time('read');
  let changes = await fschanges.getEventsSince(dir, dir + '/token.txt', {
    ignore: [dir + '/.git']
  });
  console.timeEnd('read');
  console.log(changes);

  console.time('write');
  await fschanges.writeSnapshot(dir, dir + '/token.txt', {
    ignore: [dir + '/.git']
  });
  console.timeEnd('write');
}

run();
