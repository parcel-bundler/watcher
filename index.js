const fschanges = require('./');
const fs = require('fs');

let dir = process.cwd();

async function run() {
  try {
    let changes = await fschanges.getEventsSince(dir, dir + '/token.txt');
    console.log(changes);
  } catch (err) {
    console.log(err)
  }

  await fschanges.writeSnapshot(dir, dir + '/token.txt');
}

run();
