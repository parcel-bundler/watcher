const fschanges = require('./');
const fs = require('fs');

async function run() {
  try {
    let token = fs.readFileSync(__dirname + '/token.txt', 'utf8');
    let changes = await fschanges.getEventsSince(__dirname + '/src', token);
    console.log(changes);
  } catch (err) {
    console.log(err)
  }

  let token = await fschanges.getCurrentToken(__dirname + '/src');
  console.log('token', token)
  fs.writeFileSync(__dirname + '/token.txt', token);
}

run();
