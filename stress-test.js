const watcher = require('./');
const path = require('path');
const fs = require('fs-extra');
const SegfaultHandler = require('segfault-handler');

SegfaultHandler.registerHandler('crash.log');

let backend = [];
if (process.platform === 'darwin') {
  backends = ['fs-events'];
} else if (process.platform === 'linux') {
  backends = ['inotify'];
} else if (process.platform === 'win32') {
  backends = ['windows'];
}

async function run(backend) {
  console.log('Run Stress Test For:', backend);

  console.log('Initialising...');
  let stressTestDir = path.join(__dirname, 'stress-test');
  await fs.ensureDir(stressTestDir);
  await watcher.subscribe(stressTestDir, () => {}, {
    backend,
  });

  let currentRunId = 0;
  while (true) {
    console.log('Running stress test:', currentRunId);
    await fs.remove(stressTestDir);
    await Promise.all(
      new Array(25).fill('').map(async (_, dirName) => {
        let dir = path.join(stressTestDir, dirName.toString(10));
        for (let filename = 0; filename < 100; filename++) {
          let filepath = path.join(dir, filename.toString(10));
          await fs.outputFile(filepath, '');
        }
        await fs.remove(dir);
      }),
    );
    currentRunId++;
  }
}

for (let backend of backends) {
  run(backend);
}
