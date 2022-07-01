const watcher = require('../');
const assert = require('assert');
const fs = require('fs-extra');
const path = require('path');

let winfs;
if (process.platform === 'win32') {
  winfs = require('@gyselroth/windows-fsstat');
}

const snapshotPath = path.join(__dirname, 'snapshot.txt');
const tmpDir = path.join(
  fs.realpathSync(require('os').tmpdir()),
  Math.random().toString(31).slice(2),
);
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
const getFilename = (...dir) =>
  path.join(tmpDir, ...dir, `test${c++}${Math.random().toString(31).slice(2)}`);

function testPrecision() {
  let f = getFilename();
  fs.writeFileSync(f, '.');
  let stat = fs.statSync(f);
  return ((stat.atimeMs / 1000) | 0) === stat.atimeMs / 1000;
}

const isSecondPrecision = testPrecision();

const getMetadata = async (p) => {
  // XXX: Use lstat to get stats of symlinks rather than their targets
  if (process.platform === 'win32') {
    const stats = winfs.lstatSync(p);
    return {
      fileId: stats.fileid,
      kind: stats.directory ? 'directory' : 'file',
    };
  } else {
    const stats = await fs.lstat(p);
    return {
      ino: stats.ino,
      kind: stats.isDirectory() ? 'directory' : 'file',
    };
  }
};

const event = (e, {backend}) => {
  if (process.platform === 'win32') {
    // XXX: ino is not returned with emitted events on Windows
    delete e.ino;
  } else {
    // XXX: fileId is only returned with emitted events on Windows
    delete e.fileId;
  }

  if (backend === 'watchman') {
    // XXX: fileId is not returned with emitted events by WatchmanBackend
    delete e.fileId;
  }

  return e;
};

describe('since', () => {
  const sleep = (ms = 20) => new Promise((resolve) => setTimeout(resolve, ms));

  before(async () => {
    // wait for tmp dir to be created.
    await sleep();
  });

  after(async () => {
    try {
      await fs.unlink(snapshotPath);
      await fs.rmdir(tmpDir, {recursive: true});
    } catch (err) {}
  });

  backends.forEach((backend) => {
    describe(backend, () => {
      describe('files', () => {
        it('should emit when a file is created', async function () {
          this.timeout(5000);
          let f = getFilename();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});
          if (isSecondPrecision) {
            await sleep(1000);
          }
          await fs.writeFile(f, 'hello world');
          let {ino, fileId, kind} = await getMetadata(f);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          assert.deepEqual(res, [
            event({type: 'create', path: f, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a file is updated', async () => {
          let f = getFilename();
          await fs.writeFile(f, 'hello world');
          await sleep();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.writeFile(f, 'hi');
          let {ino, fileId, kind} = await getMetadata(f);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          assert.deepEqual(res, [
            event({type: 'update', path: f, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a file is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await sleep();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});

          let {ino, fileId, kind} = await getMetadata(f1);
          await fs.rename(f1, f2);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          if (backend === 'inotify' || backend === 'windows') {
            assert.deepEqual(res, [
              event(
                {type: 'rename', oldPath: f1, path: f2, kind, ino, fileId},
                {backend},
              ),
            ]);
          } else {
            assert.deepEqual(res, [
              event({type: 'delete', path: f1, ino, fileId, kind}, {backend}),
              event({type: 'create', path: f2, ino, fileId, kind}, {backend}),
            ]);
          }
        });

        it('should emit when a file is deleted', async () => {
          let f = getFilename();
          await fs.writeFile(f, 'hello world');
          await sleep();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});

          let {ino, fileId, kind} = await getMetadata(f);
          await fs.unlink(f);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          assert.deepEqual(res, [
            event({type: 'delete', path: f, ino, fileId, kind}, {backend}),
          ]);
        });
      });

      describe('directories', () => {
        it('should emit when a directory is created', async () => {
          let f1 = getFilename();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});
          await fs.mkdir(f1);
          let {ino, fileId, kind} = await getMetadata(f1);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          assert.deepEqual(res, [
            event({type: 'create', path: f1, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a directory is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          await fs.mkdir(f1);
          await sleep();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});

          let {ino, fileId, kind} = await getMetadata(f1);
          await fs.rename(f1, f2);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });

          if (backend === 'inotify' || backend === 'windows') {
            assert.deepEqual(res, [
              event(
                {type: 'rename', oldPath: f1, path: f2, ino, fileId, kind},
                {backend},
              ),
            ]);
          } else {
            assert.deepEqual(res, [
              event({type: 'delete', path: f1, ino, fileId, kind}, {backend}),
              event({type: 'create', path: f2, ino, fileId, kind}, {backend}),
            ]);
          }
        });

        it('should emit when a directory is deleted', async () => {
          let f1 = getFilename();
          await fs.mkdir(f1);
          await sleep();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});

          let {ino, fileId, kind} = await getMetadata(f1);
          await fs.remove(f1);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });

          assert.deepEqual(res, [
            event({type: 'delete', path: f1, ino, fileId, kind}, {backend}),
          ]);
        });
      });

      describe('sub-files', () => {
        it('should emit when a sub-file is created', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await sleep();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});
          if (isSecondPrecision) {
            await sleep(1000);
          }

          await fs.writeFile(f2, 'hello world');
          let {ino, fileId, kind} = await getMetadata(f2);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          assert.deepEqual(res, [
            event({type: 'create', path: f2, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a sub-file is updated', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await fs.writeFile(f2, 'hello world');
          await sleep();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.writeFile(f2, 'hi');
          let {ino, fileId, kind} = await getMetadata(f2);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          assert.deepEqual(res, [
            event({type: 'update', path: f2, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a sub-file is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          let f3 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await fs.writeFile(f2, 'hello world');
          await sleep();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});

          let {ino, fileId, kind} = await getMetadata(f2);
          await fs.rename(f2, f3);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          if (backend === 'inotify' || backend === 'windows') {
            assert.deepEqual(res, [
              event(
                {type: 'rename', oldPath: f2, path: f3, ino, fileId, kind},
                {backend},
              ),
            ]);
          } else {
            assert.deepEqual(res, [
              event({type: 'delete', path: f2, ino, fileId, kind}, {backend}),
              event({type: 'create', path: f3, ino, fileId, kind}, {backend}),
            ]);
          }
        });

        it('should emit when a sub-file is deleted', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await fs.writeFile(f2, 'hello world');
          await sleep();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});

          let {ino, fileId, kind} = await getMetadata(f2);
          await fs.unlink(f2);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          assert.deepEqual(res, [
            event({type: 'delete', path: f2, ino, fileId, kind}, {backend}),
          ]);
        });
      });

      describe('sub-directories', () => {
        it('should emit when a sub-directory is created', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await sleep();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.mkdir(f2);
          let {ino, fileId, kind} = await getMetadata(f2);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          assert.deepEqual(res, [
            event({type: 'create', path: f2, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a sub-directory is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          let f3 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await fs.mkdir(f2);
          await sleep();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});

          let {ino, fileId, kind} = await getMetadata(f2);
          await fs.rename(f2, f3);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          if (backend === 'inotify' || backend === 'windows') {
            assert.deepEqual(res, [
              event(
                {type: 'rename', oldPath: f2, path: f3, ino, fileId, kind},
                {backend},
              ),
            ]);
          } else {
            assert.deepEqual(res, [
              event({type: 'delete', path: f2, ino, fileId, kind}, {backend}),
              event({type: 'create', path: f3, ino, fileId, kind}, {backend}),
            ]);
          }
        });

        it('should emit when a sub-directory is deleted with files inside', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await fs.writeFile(f2, 'hello world');
          await sleep();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});

          let {
            ino: f1Ino,
            fileId: f1FileId,
            kind: f1Kind,
          } = await getMetadata(f1);
          let {
            ino: f2Ino,
            fileId: f2FileId,
            kind: f2Kind,
          } = await getMetadata(f2);
          await fs.remove(f1);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          try {
            assert.deepEqual(res, [
              event(
                {
                  type: 'delete',
                  path: f2,
                  ino: f2Ino,
                  fileId: f2FileId,
                  kind: f2Kind,
                },
                {backend},
              ),
              event(
                {
                  type: 'delete',
                  path: f1,
                  ino: f1Ino,
                  fileId: f1FileId,
                  kind: f1Kind,
                },
                {backend},
              ),
            ]);
          } catch (err) {
            // XXX: when deleting a directory and its content, events can be
            // notified in either order.
            assert.deepEqual(res, [
              event(
                {
                  type: 'delete',
                  path: f1,
                  ino: f1Ino,
                  fileId: f1FileId,
                  kind: f1Kind,
                },
                {backend},
              ),
              event(
                {
                  type: 'delete',
                  path: f2,
                  ino: f2Ino,
                  fileId: f2FileId,
                  kind: f2Kind,
                },
                {backend},
              ),
            ]);
          }
        });
      });

      describe('symlinks', () => {
        it('should emit when a symlink is created', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await sleep();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.symlink(f1, f2);
          let {ino, fileId, kind} = await getMetadata(f2);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          assert.deepEqual(res, [
            event({type: 'create', path: f2, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a symlink is updated', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await fs.symlink(f1, f2);
          await sleep();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});

          let {ino, fileId, kind} = await getMetadata(f1);
          await fs.writeFile(f2, 'hi');
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          assert.deepEqual(res, [
            event({type: 'update', path: f1, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a symlink is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          let f3 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await fs.symlink(f1, f2);
          await sleep();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});

          let {ino, fileId, kind} = await getMetadata(f2);
          await fs.rename(f2, f3);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          if (backend === 'inotify' || backend === 'windows') {
            assert.deepEqual(res, [
              event(
                {type: 'rename', oldPath: f2, path: f3, ino, fileId, kind},
                {backend},
              ),
            ]);
          } else {
            assert.deepEqual(res, [
              event({type: 'delete', path: f2, ino, fileId, kind}, {backend}),
              event({type: 'create', path: f3, ino, fileId, kind}, {backend}),
            ]);
          }
        });

        it('should emit when a symlink is deleted', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await fs.symlink(f1, f2);
          await sleep();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});

          let {ino, fileId, kind} = await getMetadata(f2);
          await fs.unlink(f2);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          assert.deepEqual(res, [
            event({type: 'delete', path: f2, ino, fileId, kind}, {backend}),
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
            await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});
            await fs.writeFile(f1, 'hello world');
            let {ino, fileId, kind} = await getMetadata(f1);
            await fs.writeFile(f2, 'hello world');
            await fs.unlink(f2);
            await sleep();

            let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
              backend,
            });
            assert.deepEqual(res, [
              event({type: 'create', path: f1, ino, fileId, kind}, {backend}),
            ]);
          });
        }

        it('should coalese create and update events', async () => {
          let f1 = getFilename();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});
          if (isSecondPrecision) {
            await sleep(1000);
          }
          await fs.writeFile(f1, 'hello world');
          await fs.writeFile(f1, 'updated');
          let {ino, fileId, kind} = await getMetadata(f1);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          assert.deepEqual(res, [
            event({type: 'create', path: f1, ino, fileId, kind}, {backend}),
          ]);
        });

        if (backend !== 'fs-events') {
          it('should coalese create and rename events', async () => {
            let f1 = getFilename();
            let f2 = getFilename();
            await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});
            await fs.writeFile(f1, 'hello world');
            let {ino, fileId, kind} = await getMetadata(f1);
            await fs.rename(f1, f2);
            await sleep();

            let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
              backend,
            });
            assert.deepEqual(res, [
              event({type: 'create', path: f2, ino, fileId, kind}, {backend}),
            ]);
          });

          it('should coalese multiple rename events', async () => {
            let f1 = getFilename();
            let f2 = getFilename();
            let f3 = getFilename();
            let f4 = getFilename();
            await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});
            await fs.writeFile(f1, 'hello world');
            let {ino, fileId, kind} = await getMetadata(f1);
            await fs.rename(f1, f2);
            await fs.rename(f2, f3);
            await fs.rename(f3, f4);
            await sleep();

            let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
              backend,
            });
            assert.deepEqual(res, [
              event({type: 'create', path: f4, ino, fileId, kind}, {backend}),
            ]);
          });
        }

        it('should coalese multiple update events', async () => {
          let f1 = getFilename();
          await fs.writeFile(f1, 'hello world');
          let {ino, fileId, kind} = await getMetadata(f1);
          await sleep();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.writeFile(f1, 'update');
          await fs.writeFile(f1, 'update2');
          await fs.writeFile(f1, 'update3');
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          assert.deepEqual(res, [
            event({type: 'update', path: f1, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should coalese update and delete events', async () => {
          let f1 = getFilename();
          await fs.writeFile(f1, 'hello world');
          let {ino, fileId, kind} = await getMetadata(f1);
          await sleep();
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend});

          await fs.writeFile(f1, 'update');
          await fs.unlink(f1);
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
          });
          assert.deepEqual(res, [
            event({type: 'delete', path: f1, ino, fileId, kind}, {backend}),
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
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend, ignore});
          if (isSecondPrecision) {
            await sleep(1000);
          }

          await fs.writeFile(f1, 'hello');
          let {ino, fileId, kind} = await getMetadata(f1);
          await fs.writeFile(f2, 'sup');
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
            ignore,
          });
          assert.deepEqual(res, [
            event({type: 'create', path: f1, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should ignore a file', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          let ignore = [f2];
          await watcher.writeSnapshot(tmpDir, snapshotPath, {backend, ignore});
          if (isSecondPrecision) {
            await sleep(1000);
          }

          await fs.writeFile(f1, 'hello');
          let {ino, fileId, kind} = await getMetadata(f1);
          await fs.writeFile(f2, 'sup');
          await sleep();

          let res = await watcher.getEventsSince(tmpDir, snapshotPath, {
            backend,
            ignore,
          });
          assert.deepEqual(res, [
            event({type: 'create', path: f1, ino, fileId, kind}, {backend}),
          ]);
        });
      });

      describe('errors', () => {
        it('should error if the watched directory does not exist', async () => {
          let dir = path.join(
            fs.realpathSync(require('os').tmpdir()),
            Math.random().toString(31).slice(2),
          );

          let threw = false;
          try {
            await watcher.writeSnapshot(dir, snapshotPath, {backend});
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

          let file = path.join(
            fs.realpathSync(require('os').tmpdir()),
            Math.random().toString(31).slice(2),
          );
          fs.writeFileSync(file, 'test');

          let threw = false;
          try {
            await watcher.writeSnapshot(file, snapshotPath, {backend});
          } catch (err) {
            threw = true;
          }

          assert(threw, 'did not throw');
        });
      });

      describe('readTree', () => {
        it('should store UTF-8 paths properly in the tree', async () => {
          let dir = getFilename();
          await fs.mkdir(dir);
          let f = path.join(dir, 'spécial');
          await fs.writeFile(f, 'hello');
          let {ino, fileId, kind} = await getMetadata(f);

          async function listen() {
            let cbs = [];
            let nextEvent = () => {
              return new Promise((resolve) => {
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
            let sub = await watcher.subscribe(dir, fn, {backend});

            return [nextEvent, sub];
          }

          let [nextEvent, sub] = await listen(dir);
          try {
            await watcher.writeSnapshot(dir, snapshotPath, {backend});

            await fs.remove(f);

            // XXX: no events emitted if non-ascii characters are not handled
            // properly in BruteForceBackend::readTree on Windows.
            let res = await nextEvent();
            assert.deepEqual(res, [
              event({type: 'delete', path: f, ino, fileId, kind}, {backend}),
            ]);
          } finally {
            await sub.unsubscribe();
          }
        });
      });
    });
  });
});
