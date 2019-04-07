const fschanges = require('../');
const assert = require('assert');
const fs = require('fs-extra');
const path = require('path');
const {execSync} = require('child_process');

let backends = [];
if (process.platform === 'darwin') {
  backends = ['fs-events', 'watchman'];
} else if (process.platform === 'linux') {
  backends = ['inotify', 'watchman'];
} else if (process.platform === 'win32') {
  backends = ['windows', 'watchman'];
}

describe('watcher', () => {
  backends.forEach(backend => {
    describe(backend, () => {
      let tmpDir;
      let cbs = [];
      let subscribed = false;
      let nextEvent = () => {
        return new Promise(resolve => {
          cbs.push(resolve);
        });
      };

      let fn = (err, events) => {
        if (err) {
          throw err;
        }
        
        setImmediate(() => {
          for (let cb of cbs) {
            cb(events);
          }

          cbs = [];
        });
      };

      let c = 0;
      const getFilename = (...dir) => path.join(tmpDir, ...dir, `test${c++}${Math.random().toString(31).slice(2)}`);
      let ignoreDir, ignoreFile, sub;

      before(async () => {
        tmpDir = path.join(fs.realpathSync(require('os').tmpdir()), Math.random().toString(31).slice(2));
        fs.mkdirpSync(tmpDir);
        ignoreDir = getFilename();
        ignoreFile = getFilename();
        await new Promise(resolve => setTimeout(resolve, 100));
        sub = await fschanges.subscribe(tmpDir, fn, {backend, ignore: [ignoreDir, ignoreFile]});
      });

      after(async () => {
        await sub.unsubscribe();
      });

      describe('files', () => {
        it('should emit when a file is created', async () => {
          let f = getFilename();
          fs.writeFile(f, 'hello world');
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: f}
          ]);
        });

        it('should emit when a file is updated', async () => {
          let f = getFilename();
          fs.writeFile(f, 'hello world');
          await nextEvent();

          fs.writeFile(f, 'hi');
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'update', path: f}
          ]);
        });

        it('should emit when a file is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          fs.writeFile(f1, 'hello world');
          await nextEvent();

          fs.rename(f1, f2);
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'delete', path: f1},
            {type: 'create', path: f2}
          ]);
        });

        it('should emit when a file is deleted', async () => {
          let f = getFilename();
          fs.writeFile(f, 'hello world');
          await nextEvent();

          fs.unlink(f);
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'delete', path: f}
          ]);
        });
      });

      describe('directories', () => {
        it('should emit when a directory is created', async () => {
          let f = getFilename();
          fs.mkdir(f);
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: f}
          ]);
        });

        it('should emit when a directory is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          fs.mkdir(f1);
          await nextEvent();

          fs.rename(f1, f2);
          let res = await nextEvent();

          assert.deepEqual(res, [
            {type: 'delete', path: f1},
            {type: 'create', path: f2}
          ]);
        });

        it('should emit when a directory is deleted', async () => {
          let f = getFilename();
          fs.mkdir(f);
          await nextEvent();

          fs.remove(f);
          let res = await nextEvent();

          assert.deepEqual(res, [
            {type: 'delete', path: f}
          ]);
        });
      });

      describe('sub-files', () => {
        it('should emit when a sub-file is created', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          fs.mkdir(f1);
          await nextEvent();

          fs.writeFile(f2, 'hello world');
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: f2}
          ]);
        });

        it('should emit when a sub-file is updated', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          fs.mkdir(f1);
          await nextEvent();

          fs.writeFile(f2, 'hello world');
          await nextEvent();

          fs.writeFile(f2, 'hi');
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'update', path: f2}
          ]);
        });

        it('should emit when a sub-file is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          let f3 = getFilename(path.basename(f1));
          fs.mkdir(f1);
          await nextEvent();

          fs.writeFile(f2, 'hello world');
          await nextEvent();

          fs.rename(f2, f3);
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'delete', path: f2},
            {type: 'create', path: f3}
          ]);
        });

        it('should emit when a sub-file is deleted', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          fs.mkdir(f1);
          await nextEvent();
          
          fs.writeFile(f2, 'hello world');
          await nextEvent();

          fs.unlink(f2);
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'delete', path: f2}
          ]);
        });
      });

      describe('sub-directories', () => {
        it('should emit when a sub-directory is created', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          fs.mkdir(f1);
          await nextEvent();

          fs.mkdir(f2);
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: f2}
          ]);
        });

        it('should emit when a sub-directory is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          let f3 = getFilename(path.basename(f1));
          fs.mkdir(f1);
          await nextEvent();

          fs.mkdir(f2);
          await nextEvent();

          fs.rename(f2, f3);
          let res = await nextEvent();

          assert.deepEqual(res, [
            {type: 'delete', path: f2},
            {type: 'create', path: f3}
          ]);
        });

        it('should emit when a sub-directory is deleted with files inside', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          fs.mkdir(f1);
          await nextEvent();
          
          fs.writeFile(f2, 'hello world');
          await nextEvent();

          fs.remove(f1);
          let res = await nextEvent();
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
          fs.writeFile(f1, 'hello world');
          await nextEvent();

          fs.symlink(f1, f2);

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: f2}
          ]);
        });

        it('should emit when a symlink is updated', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          fs.writeFile(f1, 'hello world');
          await nextEvent();

          fs.symlink(f1, f2);
          await nextEvent();

          fs.writeFile(f2, 'hi');

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'update', path: f1}
          ]);
        });

        it('should emit when a symlink is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          let f3 = getFilename();
          fs.writeFile(f1, 'hello world');
          await nextEvent();

          fs.symlink(f1, f2);
          await nextEvent();

          fs.rename(f2, f3);

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'delete', path: f2},
            {type: 'create', path: f3}
          ]);
        });

        it('should emit when a symlink is deleted', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          fs.writeFile(f1, 'hello world');
          await nextEvent();

          fs.symlink(f1, f2);
          await nextEvent();

          fs.unlink(f2);

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'delete', path: f2}
          ]);
        });
      });

      describe('rapid changes', () => {
        it('should ignore files that are created and deleted rapidly', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await fs.writeFile(f2, 'hello world');
          fs.unlink(f2);

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: f1}
          ]);
        });

        it('should coalese create and update events', async () => {
          let f1 = getFilename();
          await fs.writeFile(f1, 'hello world');
          fs.writeFile(f1, 'updated');

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: f1}
          ]);
        });

        it('should coalese create and rename events', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          await fs.writeFile(f1, 'hello world');
          fs.rename(f1, f2);

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: f2}
          ]);
        });

        it('should coalese multiple rename events', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          let f3 = getFilename();
          let f4 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await fs.rename(f1, f2);
          await fs.rename(f2, f3);
          fs.rename(f3, f4);

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: f4}
          ]);
        });

        it('should coalese multiple update events', async () => {
          let f1 = getFilename();
          fs.writeFile(f1, 'hello world');
          await nextEvent();

          await fs.writeFile(f1, 'update');
          await fs.writeFile(f1, 'update2');
          fs.writeFile(f1, 'update3');

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'update', path: f1}
          ]);
        });

        it('should coalese update and delete events', async () => {
          let f1 = getFilename();
          fs.writeFile(f1, 'hello world');
          await nextEvent();

          await fs.writeFile(f1, 'update');
          fs.unlink(f1);

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'delete', path: f1}
          ]);
        });
      });

      describe('ignore', () => {
        it('should ignore a directory', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(ignoreDir));
          await fs.mkdir(ignoreDir);

          fs.writeFile(f1, 'hello');
          fs.writeFile(f2, 'sup');

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: f1}
          ]);
        });

        it('should ignore a file', async () => {
          let f1 = getFilename();

          fs.writeFile(f1, 'hello');
          fs.writeFile(ignoreFile, 'sup');

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: f1}
          ]);
        });
      });

      describe('multiple', () => {
        it('should support multiple watchers for the same directory', async () => {
          let dir = path.join(fs.realpathSync(require('os').tmpdir()), Math.random().toString(31).slice(2));
          fs.mkdirpSync(dir);
          await new Promise(resolve => setTimeout(resolve, 100));

          function listen() {
            return new Promise(async resolve => {
              let sub = await fschanges.subscribe(dir, async (err, events) => {
                setImmediate(() => resolve(events));
                await sub.unsubscribe();
              }, {backend});
            });
          }

          let l1 = listen();
          let l2 = listen();
          await new Promise(resolve => setTimeout(resolve, 100));

          fs.writeFile(path.join(dir, 'test1.txt'), 'test1');

          let res = await Promise.all([l1, l2]);
          assert.deepEqual(res, [
            [{type: 'create', path: path.join(dir, 'test1.txt')}],
            [{type: 'create', path: path.join(dir, 'test1.txt')}]
          ]);
        });

        it('should support multiple watchers for the same directory with different ignore paths', async () => {
          let dir = path.join(fs.realpathSync(require('os').tmpdir()), Math.random().toString(31).slice(2));
          fs.mkdirpSync(dir);
          await new Promise(resolve => setTimeout(resolve, 100));

          function listen(ignore) {
            return new Promise(async resolve => {
              let sub = await fschanges.subscribe(dir, async (err, events) => {
                setImmediate(() => resolve(events));
                await sub.unsubscribe();
              }, {backend, ignore});
            });
          }

          let l1 = listen([path.join(dir, 'test1.txt')]);
          let l2 = listen([path.join(dir, 'test2.txt')]);
          await new Promise(resolve => setTimeout(resolve, 100));

          fs.writeFile(path.join(dir, 'test1.txt'), 'test1');
          fs.writeFile(path.join(dir, 'test2.txt'), 'test1');

          let res = await Promise.all([l1, l2]);
          assert.deepEqual(res, [
            [{type: 'create', path: path.join(dir, 'test2.txt')}],
            [{type: 'create', path: path.join(dir, 'test1.txt')}]
          ]);
        });

        it('should support multiple watchers for different directories', async () => {
          let dir1 = path.join(fs.realpathSync(require('os').tmpdir()), Math.random().toString(31).slice(2));
          let dir2 = path.join(fs.realpathSync(require('os').tmpdir()), Math.random().toString(31).slice(2));
          fs.mkdirpSync(dir1);
          fs.mkdirpSync(dir2);
          await new Promise(resolve => setTimeout(resolve, 100));

          function listen(dir) {
            return new Promise(async resolve => {
              let sub = await fschanges.subscribe(dir, async (err, events) => {
                setImmediate(() => resolve(events));
                await sub.unsubscribe();
              }, {backend});
            });
          }

          let l1 = listen(dir1);
          let l2 = listen(dir2);
          await new Promise(resolve => setTimeout(resolve, 100));

          fs.writeFile(path.join(dir1, 'test1.txt'), 'test1');
          fs.writeFile(path.join(dir2, 'test1.txt'), 'test1');

          let res = await Promise.all([l1, l2]);
          assert.deepEqual(res, [
            [{type: 'create', path: path.join(dir1, 'test1.txt')}],
            [{type: 'create', path: path.join(dir2, 'test1.txt')}]
          ]);
        });

        it('should work when getting events since a snapshot on an already watched directory', async () => {
          let dir = path.join(fs.realpathSync(require('os').tmpdir()), Math.random().toString(31).slice(2));
          let snapshot = path.join(fs.realpathSync(require('os').tmpdir()), Math.random().toString(31).slice(2));
          fs.mkdirpSync(dir);
          await new Promise(resolve => setTimeout(resolve, 100));

          function listen(dir) {
            return new Promise(async resolve => {
              let sub = await fschanges.subscribe(dir, (err, events) => {
                setImmediate(() => resolve([events, sub]));
              }, {backend});
            });
          }

          let l = listen(dir);
          await new Promise(resolve => setTimeout(resolve, 100));

          await fs.writeFile(path.join(dir, 'test1.txt'), 'hello1');
          await new Promise(resolve => setTimeout(resolve, 100));

          await fschanges.writeSnapshot(dir, snapshot, {backend});
          await new Promise(resolve => setTimeout(resolve, 1000));

          await fs.writeFile(path.join(dir, 'test2.txt'), 'hello2');
          await new Promise(resolve => setTimeout(resolve, 100));

          let [watched, sub] = await l;
          assert.deepEqual(watched, [
            {type: 'create', path: path.join(dir, 'test1.txt')}
          ]);

          let since = await fschanges.getEventsSince(dir, snapshot, {backend});
          assert.deepEqual(since, [
            {type: 'create', path: path.join(dir, 'test2.txt')}
          ]);

          await sub.unsubscribe();
        });
      });

      describe('errors', () => {
        it('should error if the watched directory does not exist', async () => {
          let dir = path.join(fs.realpathSync(require('os').tmpdir()), Math.random().toString(31).slice(2));

          let threw = false;
          try {
            await fschanges.subscribe(dir, (err, events) => {
              assert(false, 'Should not get here');
            }, {backend});
          } catch (err) {
            threw = true;
          }

          assert(threw, 'did not throw');
        });

        it('should error if the watched path is not a directory', async () => {
          if (backend === 'watchman' && process.platform === 'win32') {
            // There is a bug in watchman on windows where the `watch` command hangs if the path is not a directory.
            return;
          }
          
          let file = path.join(fs.realpathSync(require('os').tmpdir()), Math.random().toString(31).slice(2));
          fs.writeFileSync(file, 'test');

          let threw = false;
          try {
            await fschanges.subscribe(file, (err, events) => {
              assert(false, 'Should not get here');
            }, {backend});
          } catch (err) {
            threw = true;
          }

          assert(threw, 'did not throw');
        });
      });
    });
  });

  describe('watchman errors', () => {
    it('should emit an error when watchman dies', async () => {
      let dir = path.join(fs.realpathSync(require('os').tmpdir()), Math.random().toString(31).slice(2));
      fs.mkdirpSync(dir);
      await new Promise(resolve => setTimeout(resolve, 100));

      let p = new Promise(resolve => {
        fschanges.subscribe(dir, (err, events) => {
          setImmediate(() => resolve(err));
        }, {backend: 'watchman'});
      });

      execSync('watchman shutdown-server');

      let err = await p;
      assert(err, 'No error was emitted');
    });
  });
});
