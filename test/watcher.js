const fschanges = require('../');
const assert = require('assert');
const fs = require('fs-extra');
const path = require('path');

const tmpDir = path.join(__dirname, 'tmp');
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
      let cbs = [];
      let subscribed = false;
      let nextEvent = () => {
        return new Promise(resolve => {
          cbs.push(resolve);
        });
      };

      let fn = events => {
        // console.log(events);
        setImmediate(() => {
          for (let cb of cbs) {
            cb(events);
          }

          cbs = [];
        });
      };

      before(async () => {
        await fs.mkdirp(tmpDir);
        await new Promise(resolve => setTimeout(resolve, 200));
        fschanges.subscribe(tmpDir, fn, {backend});
      });

      beforeEach(async () => {
        let isEmpty = (await fs.readdir(tmpDir)).length === 0;
        if (!isEmpty) {
          await fs.emptydir(tmpDir);
        }

        if (subscribed && !isEmpty) {
          await nextEvent();
        } else {
          subscribed = true;
        }
      });

      after(async () => {
        fschanges.unsubscribe(tmpDir, fn, {backend});
        await fs.remove(tmpDir);
      });

      describe('files', () => {
        it('should emit when a file is created', async () => {
          fs.writeFile(path.join(tmpDir, 'test.txt'), 'hello world');
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: path.join(tmpDir, 'test.txt')}
          ]);
        });

        it('should emit when a file is updated', async () => {
          fs.writeFile(path.join(tmpDir, 'test.txt'), 'hello world');
          await nextEvent();

          fs.writeFile(path.join(tmpDir, 'test.txt'), 'hi');
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'update', path: path.join(tmpDir, 'test.txt')}
          ]);
        });

        it('should emit when a file is renamed', async () => {
          fs.writeFile(path.join(tmpDir, 'test.txt'), 'hello world');
          await nextEvent();

          fs.rename(path.join(tmpDir, 'test.txt'), path.join(tmpDir, 'test2.txt'));
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'delete', path: path.join(tmpDir, 'test.txt')},
            {type: 'create', path: path.join(tmpDir, 'test2.txt')}
          ]);
        });

        it('should emit when a file is deleted', async () => {
          fs.writeFile(path.join(tmpDir, 'test.txt'), 'hello world');
          await nextEvent();

          fs.unlink(path.join(tmpDir, 'test.txt'));
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'delete', path: path.join(tmpDir, 'test.txt')}
          ]);
        });
      });

      describe('directories', () => {
        it('should emit when a directory is created', async () => {
          fs.mkdir(path.join(tmpDir, 'dir'));
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: path.join(tmpDir, 'dir')}
          ]);
        });

        it('should emit when a directory is renamed', async () => {
          fs.mkdir(path.join(tmpDir, 'dir'));
          await nextEvent();

          fs.rename(path.join(tmpDir, 'dir'), path.join(tmpDir, 'dir2'));
          let res = await nextEvent();

          assert.deepEqual(res, [
            {type: 'delete', path: path.join(tmpDir, 'dir')},
            {type: 'create', path: path.join(tmpDir, 'dir2')}
          ]);
        });

        it('should emit when a directory is deleted', async () => {
          fs.mkdir(path.join(tmpDir, 'dir'));
          await nextEvent();

          fs.remove(path.join(tmpDir, 'dir'));
          let res = await nextEvent();

          assert.deepEqual(res, [
            {type: 'delete', path: path.join(tmpDir, 'dir')}
          ]);
        });
      });

      describe('sub-files', () => {
        it('should emit when a sub-file is created', async () => {
          fs.mkdir(path.join(tmpDir, 'dir'));
          await nextEvent();

          fs.writeFile(path.join(tmpDir, 'dir', 'test.txt'), 'hello world');
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: path.join(tmpDir, 'dir', 'test.txt')}
          ]);
        });

        it('should emit when a sub-file is updated', async () => {
          fs.mkdir(path.join(tmpDir, 'dir'));
          await nextEvent();

          fs.writeFile(path.join(tmpDir, 'dir', 'test.txt'), 'hello world');
          await nextEvent();

          fs.writeFile(path.join(tmpDir, 'dir', 'test.txt'), 'hi');
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'update', path: path.join(tmpDir, 'dir', 'test.txt')}
          ]);
        });

        it('should emit when a sub-file is renamed', async () => {
          fs.mkdir(path.join(tmpDir, 'dir'));
          await nextEvent();

          fs.writeFile(path.join(tmpDir, 'dir', 'test.txt'), 'hello world');
          await nextEvent();

          fs.rename(path.join(tmpDir, 'dir', 'test.txt'), path.join(tmpDir, 'dir', 'test2.txt'));
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'delete', path: path.join(tmpDir, 'dir', 'test.txt')},
            {type: 'create', path: path.join(tmpDir, 'dir', 'test2.txt')}
          ]);
        });

        it('should emit when a sub-file is deleted', async () => {
          fs.mkdir(path.join(tmpDir, 'dir'));
          await nextEvent();
          
          fs.writeFile(path.join(tmpDir, 'dir', 'test.txt'), 'hello world');
          await nextEvent();

          fs.unlink(path.join(tmpDir, 'dir', 'test.txt'));
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'delete', path: path.join(tmpDir, 'dir', 'test.txt')}
          ]);
        });
      });

      describe('sub-directories', () => {
        it('should emit when a sub-directory is created', async () => {
          fs.mkdir(path.join(tmpDir, 'dir'));
          await nextEvent();

          fs.mkdir(path.join(tmpDir, 'dir', 'sub'));
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: path.join(tmpDir, 'dir', 'sub')}
          ]);
        });

        it('should emit when a sub-directory is renamed', async () => {
          fs.mkdir(path.join(tmpDir, 'dir'));
          await nextEvent();

          fs.mkdir(path.join(tmpDir, 'dir', 'sub'));
          await nextEvent();

          fs.rename(path.join(tmpDir, 'dir', 'sub'), path.join(tmpDir, 'dir', 'sub2'));
          let res = await nextEvent();

          assert.deepEqual(res, [
            {type: 'delete', path: path.join(tmpDir, 'dir', 'sub')},
            {type: 'create', path: path.join(tmpDir, 'dir', 'sub2')}
          ]);
        });

        it('should emit when a sub-directory is deleted with files inside', async () => {
          fs.mkdir(path.join(tmpDir, 'dir'));
          await nextEvent();
          
          fs.writeFile(path.join(tmpDir, 'dir', 'test.txt'), 'hello world');
          await nextEvent();

          fs.remove(path.join(tmpDir, 'dir'));
          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'delete', path: path.join(tmpDir, 'dir')},
            {type: 'delete', path: path.join(tmpDir, 'dir', 'test.txt')}
          ]);
        });
      });

      describe('symlinks', () => {
        it('should emit when a symlink is created', async () => {
          fs.writeFile(path.join(tmpDir, 'test.txt'), 'hello world');
          await nextEvent();

          fs.symlink(path.join(tmpDir, 'test.txt'), path.join(tmpDir, 'test2.txt'));

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: path.join(tmpDir, 'test2.txt')}
          ]);
        });

        it('should emit when a symlink is updated', async () => {
          fs.writeFile(path.join(tmpDir, 'test.txt'), 'hello world');
          await nextEvent();

          fs.symlink(path.join(tmpDir, 'test.txt'), path.join(tmpDir, 'test2.txt'));
          await nextEvent();

          fs.writeFile(path.join(tmpDir, 'test2.txt'), 'hi');

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'update', path: path.join(tmpDir, 'test.txt')}
          ]);
        });

        it('should emit when a symlink is renamed', async () => {
          fs.writeFile(path.join(tmpDir, 'test.txt'), 'hello world');
          await nextEvent();

          fs.symlink(path.join(tmpDir, 'test.txt'), path.join(tmpDir, 'test2.txt'));
          await nextEvent();

          fs.rename(path.join(tmpDir, 'test2.txt'), path.join(tmpDir, 'test3.txt'));

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'delete', path: path.join(tmpDir, 'test2.txt')},
            {type: 'create', path: path.join(tmpDir, 'test3.txt')}
          ]);
        });

        it('should emit when a symlink is deleted', async () => {
          fs.writeFile(path.join(tmpDir, 'test.txt'), 'hello world');
          await nextEvent();

          fs.symlink(path.join(tmpDir, 'test.txt'), path.join(tmpDir, 'test2.txt'));
          await nextEvent();

          fs.unlink(path.join(tmpDir, 'test2.txt'));

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'delete', path: path.join(tmpDir, 'test2.txt')}
          ]);
        });
      });

      describe('rapid changes', () => {
        it('should ignore files that are created and deleted rapidly', async () => {
          await fs.writeFile(path.join(tmpDir, 'a.txt'), 'hello world');
          await fs.writeFile(path.join(tmpDir, 'test2.txt'), 'hello world');
          await fs.unlink(path.join(tmpDir, 'test2.txt'));

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: path.join(tmpDir, 'a.txt')}
          ]);
        });

        it('should coalese create and update events', async () => {
          await fs.writeFile(path.join(tmpDir, 'test.txt'), 'hello world');
          await fs.writeFile(path.join(tmpDir, 'test.txt'), 'updated');

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: path.join(tmpDir, 'test.txt')}
          ]);
        });

        it('should coalese create and rename events', async () => {
          await fs.writeFile(path.join(tmpDir, 'test.txt'), 'hello world');
          await fs.rename(path.join(tmpDir, 'test.txt'), path.join(tmpDir, 'test2.txt'));

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: path.join(tmpDir, 'test2.txt')}
          ]);
        });

        it('should coalese multiple rename events', async () => {
          await fs.writeFile(path.join(tmpDir, 'test.txt'), 'hello world');
          await fs.rename(path.join(tmpDir, 'test.txt'), path.join(tmpDir, 'test2.txt'));
          await fs.rename(path.join(tmpDir, 'test2.txt'), path.join(tmpDir, 'test3.txt'));
          await fs.rename(path.join(tmpDir, 'test3.txt'), path.join(tmpDir, 'test4.txt'));

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'create', path: path.join(tmpDir, 'test4.txt')}
          ]);
        });

        it('should coalese multiple update events', async () => {
          await fs.writeFile(path.join(tmpDir, 'test.txt'), 'hello world');
          await nextEvent();

          await fs.writeFile(path.join(tmpDir, 'test.txt'), 'update');
          await fs.writeFile(path.join(tmpDir, 'test.txt'), 'update2');
          await fs.writeFile(path.join(tmpDir, 'test.txt'), 'update3');

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'update', path: path.join(tmpDir, 'test.txt')}
          ]);
        });

        it('should coalese update and delete events', async () => {
          await fs.writeFile(path.join(tmpDir, 'test.txt'), 'hello world');
          await nextEvent();

          await fs.writeFile(path.join(tmpDir, 'test.txt'), 'update');
          await fs.unlink(path.join(tmpDir, 'test.txt'));

          let res = await nextEvent();
          assert.deepEqual(res, [
            {type: 'delete', path: path.join(tmpDir, 'test.txt')}
          ]);
        });
      });
    });
  });
});
