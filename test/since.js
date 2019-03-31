const fschanges = require('../');
const assert = require('assert');
const fs = require('fs-extra');
const path = require('path');

const snapshotPath = path.join(__dirname, 'snapshot.txt');
const tmpDir = path.join(fs.realpathSync(require('os').tmpdir()), Math.random().toString(31).slice(2));
fs.mkdirpSync(tmpDir);

let backends = [];
if (process.platform === 'darwin') {
  backends = ['fs-events', 'watchman'];
} else if (process.platform === 'linux') {
  backends = ['inotify', 'watchman'];
} else if (process.platform === 'win32') {
  backends = ['windows', 'watchman'];
}

let c = 0;
const getFilename = (...dir) => path.join(tmpDir, ...dir, `test${c++}${Math.random().toString(31).slice(2)}`);

function testPrecision() {
  let f = getFilename();
  fs.writeFileSync(f, '.');
  let stat = fs.statSync(f);
  return (stat.atimeMs / 1000 | 0) === stat.atimeMs / 1000;
}

const isSecondPrecision = testPrecision();

describe('since', () => {
  after(async () => {
    try {
      await fs.unlink(snapshotPath);
    } catch (err) {}
  });

  backends.forEach(backend => {
    const sleep = (ms = 10) => new Promise(resolve => setTimeout(resolve, ms));

    describe(backend, () => {
      describe('files', () => {
        it('should emit when a file is created', async () => {
          let f = getFilename();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});
          if (isSecondPrecision) {
            await sleep(1000);
          }
          await fs.writeFile(f, 'hello world');
          await sleep();
          
          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
          assert.deepEqual(res, [
            {type: 'create', path: f}
          ]);
        });

        it('should emit when a file is updated', async () => {
          let f = getFilename();
          await fs.writeFile(f, 'hello world');
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.writeFile(f, 'hi');
          await sleep();
          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
          assert.deepEqual(res, [
            {type: 'update', path: f}
          ]);
        });

        it('should emit when a file is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.rename(f1, f2);
          await sleep();
          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
          assert.deepEqual(res, [
            {type: 'delete', path: f1},
            {type: 'create', path: f2}
          ]);
        });

        it('should emit when a file is deleted', async () => {
          let f = getFilename();
          await fs.writeFile(f, 'hello world');
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.unlink(f);
          await sleep();
          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
          assert.deepEqual(res, [
            {type: 'delete', path: f}
          ]);
        });
      });

      describe('directories', () => {
        it('should emit when a directory is created', async () => {
          let f1 = getFilename();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});
          await fs.mkdir(f1);
          await sleep();

          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
          assert.deepEqual(res, [
            {type: 'create', path: f1}
          ]);
        });

        it('should emit when a directory is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          await fs.mkdir(f1);
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.rename(f1, f2);
          await sleep();
          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});

          assert.deepEqual(res, [
            {type: 'delete', path: f1},
            {type: 'create', path: f2}
          ]);
        });

        it('should emit when a directory is deleted', async () => {
          let f1 = getFilename();
          await fs.mkdir(f1);
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.remove(f1);
          await sleep();
          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});

          assert.deepEqual(res, [
            {type: 'delete', path: f1}
          ]);
        });
      });

      describe('sub-files', () => {
        it('should emit when a sub-file is created', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});
          if (isSecondPrecision) {
            await sleep(1000);
          }

          await fs.writeFile(f2, 'hello world');
          await sleep();
          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
          assert.deepEqual(res, [
            {type: 'create', path: f2}
          ]);
        });

        it('should emit when a sub-file is updated', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await fs.writeFile(f2, 'hello world');
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.writeFile(f2, 'hi');
          await sleep();
          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
          assert.deepEqual(res, [
            {type: 'update', path: f2}
          ]);
        });

        it('should emit when a sub-file is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          let f3 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await fs.writeFile(f2, 'hello world');
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.rename(f2, f3);
          await sleep();
          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
          assert.deepEqual(res, [
            {type: 'delete', path: f2},
            {type: 'create', path: f3}
          ]);
        });

        it('should emit when a sub-file is deleted', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          await fs.mkdir(f1);          
          await fs.writeFile(f2, 'hello world');
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.unlink(f2);
          await sleep();
          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
          assert.deepEqual(res, [
            {type: 'delete', path: f2}
          ]);
        });
      });

      describe('sub-directories', () => {
        it('should emit when a sub-directory is created', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.mkdir(f2);
          await sleep();
          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
          assert.deepEqual(res, [
            {type: 'create', path: f2}
          ]);
        });

        it('should emit when a sub-directory is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          let f3 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await fs.mkdir(f2);
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.rename(f2, f3);
          await sleep();
          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});

          assert.deepEqual(res, [
            {type: 'delete', path: f2},
            {type: 'create', path: f3}
          ]);
        });

        it('should emit when a sub-directory is deleted with files inside', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await fs.writeFile(f2, 'hello world');
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.remove(f1);
          await sleep();
          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
          assert.deepEqual(res, [
            {type: 'delete', path: f1},
            {type: 'delete', path: f2}
          ]);
        });
      });

      describe('symlinks', () => {
        it('should emit when a symlink is created', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.symlink(f1, f2);
          await sleep();
          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
          assert.deepEqual(res, [
            {type: 'create', path: f2}
          ]);
        });

        it('should emit when a symlink is updated', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await fs.symlink(f1, f2);
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.writeFile(f2, 'hi');
          await sleep();
          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
          assert.deepEqual(res, [
            {type: 'update', path: f1}
          ]);
        });

        it('should emit when a symlink is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          let f3 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await fs.symlink(f1, f2);
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.rename(f2, f3);
          await sleep();
          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
          assert.deepEqual(res, [
            {type: 'delete', path: f2},
            {type: 'create', path: f3}
          ]);
        });

        it('should emit when a symlink is deleted', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await fs.symlink(f1, f2);
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.unlink(f2);
          await sleep();
          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
          assert.deepEqual(res, [
            {type: 'delete', path: f2}
          ]);
        });
      });

      describe('rapid changes', () => {
        // fsevents doesn't provide the granularity to ignore rapid creates + deletes/renames
        if (backend !== 'fs-events') {
          it('should ignore files that are created and deleted rapidly', async () => {
            let f1 = getFilename();
            let f2 = getFilename();
            await sleep();
            await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});
            await fs.writeFile(f1, 'hello world');
            await fs.writeFile(f2, 'hello world');
            await fs.unlink(f2);
            await sleep();

            let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
            assert.deepEqual(res, [
              {type: 'create', path: f1}
            ]);
          });
        }

        it('should coalese create and update events', async () => {
          let f1 = getFilename();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});
          if (isSecondPrecision) {
            await sleep(1000);
          }
          await fs.writeFile(f1, 'hello world');
          await fs.writeFile(f1, 'updated');
          await sleep();

          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
          assert.deepEqual(res, [
            {type: 'create', path: f1}
          ]);
        });

        if (backend !== 'fs-events') {
          it('should coalese create and rename events', async () => {
            let f1 = getFilename();
            let f2 = getFilename();
            await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});
            await fs.writeFile(f1, 'hello world');
            await fs.rename(f1, f2);
            await sleep();

            let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
            assert.deepEqual(res, [
              {type: 'create', path: f2}
            ]);
          });

          it('should coalese multiple rename events', async () => {
            let f1 = getFilename();
            let f2 = getFilename();
            let f3 = getFilename();
            let f4 = getFilename();
            await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});
            await fs.writeFile(f1, 'hello world');
            await fs.rename(f1, f2);
            await fs.rename(f2, f3);
            await fs.rename(f3, f4);
            await sleep();

            let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
            assert.deepEqual(res, [
              {type: 'create', path: f4}
            ]);
          });
        }

        it('should coalese multiple update events', async () => {
          let f1 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.writeFile(f1, 'update');
          await fs.writeFile(f1, 'update2');
          await fs.writeFile(f1, 'update3');
          await sleep();

          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
          assert.deepEqual(res, [
            {type: 'update', path:f1}
          ]);
        });

        it('should coalese update and delete events', async () => {
          let f1 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.writeFile(f1, 'update');
          await fs.unlink(f1);
          await sleep();

          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend});
          assert.deepEqual(res, [
            {type: 'delete', path: f1}
          ]);
        });
      });

      describe('ignore', () => {
        it('should ignore a directory', async () => {
          let f1 = getFilename();
          let dir = getFilename();
          let f2 = getFilename(path.basename(dir));
          let ignore = [dir];
          await fs.mkdir(dir);
          await sleep();
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend, ignore});
          if (isSecondPrecision) {
            await sleep(1000);
          }

          await fs.writeFile(f1, 'hello');
          await fs.writeFile(f2, 'sup');
          await sleep();

          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend, ignore});
          assert.deepEqual(res, [
            {type: 'create', path: f1}
          ]);
        });

        it('should ignore a file', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          let ignore = [f2];
          await fschanges.writeSnapshot(tmpDir, snapshotPath, {backend, ignore});
          if (isSecondPrecision) {
            await sleep(1000);
          }

          await fs.writeFile(f1, 'hello');
          await fs.writeFile(f2, 'sup');
          await sleep();

          let res = await fschanges.getEventsSince(tmpDir, snapshotPath, {backend, ignore});
          assert.deepEqual(res, [
            {type: 'create', path: f1}
          ]);
        });
      });
    });
  });
});
