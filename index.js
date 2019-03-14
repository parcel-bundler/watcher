const fschanges = require('./');
const fs = require('fs');

let dir = process.cwd();

async function run() {
  try {
    let token = fs.readFileSync(dir + '/token.txt', 'utf8');
    let changes = await fschanges.getEventsSince(dir, token);
    console.log(changes);
  } catch (err) {
    console.log(err)
  }

  let token = await fschanges.getCurrentToken(dir);
  console.log('token', token)
  fs.writeFileSync(dir + '/token.txt', token);
}

run();