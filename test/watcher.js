const fschanges = require('../');
const assert = require('assert');
const fs = require('fs-extra');
const path = require('path');

let backends = [];
if (process.platform === 'darwin') {
  backends = ['fs-events', 'watchman'];
} else if (process.platform === 'linux') {
  backends = ['inotify', 'watchman'];
} else if (process.platform === 'win32') {
  backends = ['windows'];
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

      let fn = events => {
        setImmediate(() => {
          for (let cb of cbs) {
            cb(events);
          }

          cbs = [];
        });
      };

      let c = 0;
      const getFilename = (...dir) => path.join(tmpDir, ...dir, `test${c++}${Math.random().toString(31).slice(2)}`);

      before(async () => {
        tmpDir = path.join(fs.realpathSync(require('os').tmpdir()), Math.random().toString(31).slice(2));
        fs.mkdirpSync(tmpDir);
        await new Promise(resolve => setTimeout(resolve, 100));
        fschanges.subscribe(tmpDir, fn, {backend});
      });

      after(async () => {
        fschanges.unsubscribe(tmpDir, fn, {backend});
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
    });
  });
});
