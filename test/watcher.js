const watcher = require('../');
const assert = require('assert');
const fs = require('fs-extra');
const path = require('path');
const {execSync} = require('child_process');

let winfs
if (process.platform === 'win32') {
  winfs = require('@gyselroth/windows-fsstat');
}

let backends = [];
if (process.platform === 'darwin') {
  backends = ['fs-events', 'watchman'];
} else if (process.platform === 'linux') {
  backends = ['inotify', 'watchman'];
} else if (process.platform === 'win32') {
  backends = ['windows', 'watchman'];
}

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

describe('watcher', () => {
  backends.forEach((backend) => {
    describe(backend, () => {
      let tmpDir;
      let cbs = [];
      let subscribed = false;
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

      let c = 0;
      const getFilename = (...dir) =>
        path.join(
          tmpDir,
          ...dir,
          `test${c++}${Math.random().toString(31).slice(2)}`,
        );
      let ignoreDir, ignoreFile, fileToRename, dirToRename, sub;

      before(async () => {
        tmpDir = path.join(
          fs.realpathSync(require('os').tmpdir()),
          Math.random().toString(31).slice(2),
        );
        fs.mkdirpSync(tmpDir);
        ignoreDir = getFilename();
        ignoreFile = getFilename();
        fileToRename = getFilename();
        dirToRename = getFilename();
        fs.writeFileSync(fileToRename, 'hi');
        fs.mkdirpSync(dirToRename);
        await new Promise((resolve) => setTimeout(resolve, 100));
        sub = await watcher.subscribe(tmpDir, fn, {
          backend,
          ignore: [ignoreDir, ignoreFile],
        });
      });

      after(async () => {
        await sub.unsubscribe();
        await fs.rmdir(tmpDir, { recursive: true });
      });

      describe('files', () => {
        it('should emit when a file is created', async () => {
          let f = getFilename();
          await fs.writeFile(f, 'hello world');
          let {ino, fileId, kind} = await getMetadata(f);

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'create', path: f, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a file is updated', async () => {
          let f = getFilename();
          await fs.writeFile(f, 'hello world');
          await nextEvent();

          await fs.writeFile(f, 'hi');
          let {ino, fileId, kind} = await getMetadata(f);

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'update', path: f, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a file is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await nextEvent();

          let {ino, fileId, kind} = await getMetadata(f1);
          await fs.rename(f1, f2);

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'delete', path: f1, ino, fileId, kind}, {backend}),
            event({type: 'create', path: f2, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when an existing file is renamed', async () => {
          let {ino, fileId, kind} = await getMetadata(fileToRename);
          let f2 = getFilename();
          await fs.rename(fileToRename, f2);

          let res = await nextEvent();
          if (backend == 'windows') {
            assert.deepEqual(res, [
              // The WindowsBackend does not have access to the removed file
              // information and thus cannot tell us the delete event is for a
              // file or give us its fileId.
              event(
                {type: 'delete', path: fileToRename, kind: 'unknown'},
                {backend},
              ),
              event({type: 'create', path: f2, fileId, kind}, {backend}),
            ]);
          } else {
            assert.deepEqual(res, [
              event({type: 'delete', path: fileToRename, ino, kind}, {backend}),
              event({type: 'create', path: f2, ino, kind}, {backend}),
            ]);
          }
        });

        it('should emit when a file is renamed only changing case', async () => {
          let f1 = getFilename();
          let f2 = path.join(path.dirname(f1), path.basename(f1).toUpperCase());
          await fs.writeFile(f1, 'hello world');
          await nextEvent();

          let {ino, fileId, kind} = await getMetadata(f1);
          await fs.rename(f1, f2);

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'delete', path: f1, ino, fileId, kind}, {backend}),
            event({type: 'create', path: f2, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a file is deleted', async () => {
          let f = getFilename();
          await fs.writeFile(f, 'hello world');
          await nextEvent();

          let {ino, fileId, kind} = await getMetadata(f);
          fs.unlink(f);

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'delete', path: f, ino, fileId, kind}, {backend}),
          ]);
        });
      });

      describe('directories', () => {
        it('should emit when a directory is created', async () => {
          let f = getFilename();
          await fs.mkdir(f);
          let {ino, fileId, kind} = await getMetadata(f);

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'create', path: f, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a directory is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          await fs.mkdir(f1);
          await nextEvent();

          let {ino, fileId, kind} = await getMetadata(f1);
          await fs.rename(f1, f2);

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'delete', path: f1, ino, fileId, kind}, {backend}),
            event({type: 'create', path: f2, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when an existing directory is renamed', async () => {
          let {ino, fileId, kind} = await getMetadata(dirToRename);
          let f2 = getFilename();
          await fs.rename(dirToRename, f2);

          let res = await nextEvent();
          if (backend === 'windows') {
            // The WindowsBackend does not have access to the removed dir
            // information and thus cannot tell us the delete event is for a
            // directory or give us its fileId.
            assert.deepEqual(res, [
              event(
                {type: 'delete', path: dirToRename, kind: 'unknown'},
                {backend},
              ),
              event({type: 'create', path: f2, fileId, kind}, {backend}),
            ]);
          } else {
            assert.deepEqual(res, [
              event(
                {type: 'delete', path: dirToRename, ino, fileId, kind},
                {backend},
              ),
              event({type: 'create', path: f2, ino, fileId, kind}, {backend}),
            ]);
          }
        });

        it('should emit when a directory is deleted', async () => {
          let f = getFilename();
          await fs.mkdir(f);
          await nextEvent();

          let {ino, fileId, kind} = await getMetadata(f);
          await fs.remove(f);

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'delete', path: f, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should handle when the directory to watch is deleted', async () => {
          if (backend === 'watchman') {
            // Watchman doesn't handle this correctly
            return;
          }

          let dir = path.join(
            fs.realpathSync(require('os').tmpdir()),
            Math.random().toString(31).slice(2),
          );
          fs.mkdirpSync(dir);
          let { ino, kind } = await getMetadata(dir);
          await new Promise((resolve) => setTimeout(resolve, 100));

          let sub = await watcher.subscribe(dir, fn, {backend});

          try {
            fs.remove(dir);

            let res = await nextEvent();
            if (backend === 'inotify') {
              assert.deepEqual(res, [
                event({type: 'delete', path: dir, ino, kind}, {backend}),
              ]);
            } else {
              assert.deepEqual(res, [
                event({type: 'delete', path: dir, kind}, {backend}),
              ]);
            }

            fs.mkdirp(dir);
            res = await Promise.race([
              new Promise((resolve) => setTimeout(resolve, 100)),
              nextEvent(),
            ]);
            assert.equal(res, undefined);
          } finally {
            await sub.unsubscribe();
          }
        });
      });

      describe('sub-files', () => {
        it('should emit when a sub-file is created', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await nextEvent();

          await fs.writeFile(f2, 'hello world');
          let {ino, fileId, kind} = await getMetadata(f2);

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'create', path: f2, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a sub-file is updated', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await nextEvent();

          await fs.writeFile(f2, 'hello world');
          let {ino, fileId, kind} = await getMetadata(f2);
          await nextEvent();
          await fs.writeFile(f2, 'hi');

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'update', path: f2, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a sub-file is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          let f3 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await nextEvent();

          await fs.writeFile(f2, 'hello world');
          let {ino, fileId, kind} = await getMetadata(f2);
          await nextEvent();
          await fs.rename(f2, f3);

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'delete', path: f2, ino, fileId, kind}, {backend}),
            event({type: 'create', path: f3, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a sub-file is deleted', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await nextEvent();

          await fs.writeFile(f2, 'hello world');
          let {ino, fileId, kind} = await getMetadata(f2);
          await nextEvent();
          fs.unlink(f2);

          let res = await nextEvent();
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
          await nextEvent();

          await fs.mkdir(f2);
          let {ino, fileId, kind} = await getMetadata(f2);

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'create', path: f2, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a sub-directory is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          let f3 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await nextEvent();

          await fs.mkdir(f2);
          let {ino, fileId, kind} = await getMetadata(f2);
          await nextEvent();
          await fs.rename(f2, f3);

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'delete', path: f2, ino, fileId, kind}, {backend}),
            event({type: 'create', path: f3, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a sub-directory is deleted with files inside', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(f1));
          await fs.mkdir(f1);
          await nextEvent();

          await fs.writeFile(f2, 'hello world');
          await nextEvent();

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
          fs.remove(f1);

          let res = await nextEvent();
          if (backend === 'watchman') {
            // Watchman does not notify of individual actions but that changes
            // occured on some watched elements. The WatchmanBackend then
            // generates events for every changed document in path order.
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
          } else {
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
          }
        });

        it('should emit when a sub-directory is deleted with directories inside', async () => {
          let base = getFilename();
          await fs.mkdir(base);
          await nextEvent();

          let getPath = (p) => path.join(base, p);

          await fs.mkdir(getPath('dir'));
          let {
            ino: dirIno,
            fileId: dirFileId,
            kind: dirKind,
          } = await getMetadata(getPath('dir'));
          await nextEvent();
          await fs.mkdir(getPath('dir/subdir'));
          let {
            ino: subdirIno,
            fileId: subdirFileId,
            kind: subdirKind,
          } = await getMetadata(getPath('dir/subdir'));
          await nextEvent();

          await fs.rename(getPath('dir'), getPath('dir2'));
          await fs.rename(getPath('dir2/subdir'), getPath('dir2/subdir2'));

          let res = await nextEvent();
          if (backend === 'watchman') {
            // It seems that watchman emits these events in a very different
            // order from the other backends.
            assert.deepEqual(res, [
              event(
                {
                  type: 'create',
                  path: getPath('dir2/subdir2'),
                  ino: subdirIno,
                  fileId: subdirFileId,
                  kind: subdirKind,
                },
                {backend},
              ),
              event(
                {
                  type: 'delete',
                  path: getPath('dir'),
                  ino: dirIno,
                  fileId: dirFileId,
                  kind: dirKind,
                },
                {backend},
              ),
              event(
                {
                  type: 'delete',
                  path: getPath('dir/subdir'),
                  ino: subdirIno,
                  fileId: subdirFileId,
                  kind: subdirKind,
                },
                {backend},
              ),
              event(
                {
                  type: 'create',
                  path: getPath('dir2'),
                  ino: dirIno,
                  fileId: dirFileId,
                  kind: dirKind,
                },
                {backend},
              ),
            ]);
          } else if (backend === 'inotify') {
            assert.deepEqual(res, [
              event(
                {
                  type: 'delete',
                  path: getPath('dir'),
                  ino: dirIno,
                  fileId: dirFileId,
                  kind: dirKind,
                },
                {backend},
              ),
              event(
                {
                  type: 'create',
                  path: getPath('dir2'),
                  ino: dirIno,
                  fileId: dirFileId,
                  kind: dirKind,
                },
                {backend},
              ),
              // The Inotify backend doesn't have access to the removed dir
              // information and thus cannot give us its inode but it can tell
              // us this is an event for a directory.
              event(
                {type: 'delete', path: getPath('dir2/subdir'), kind: subdirKind},
                {backend},
              ),
              event(
                {
                  type: 'create',
                  path: getPath('dir2/subdir2'),
                  ino: subdirIno,
                  fileId: subdirFileId,
                  kind: subdirKind,
                },
                {backend},
              ),
            ]);
          } else {
            assert.deepEqual(res, [
              event(
                {
                  type: 'delete',
                  path: getPath('dir'),
                  ino: dirIno,
                  fileId: dirFileId,
                  kind: dirKind,
                },
                {backend},
              ),
              event(
                {
                  type: 'create',
                  path: getPath('dir2'),
                  ino: dirIno,
                  fileId: dirFileId,
                  kind: dirKind,
                },
                {backend},
              ),
              // The other backends don't have access to the removed dir
              // information and thus cannot tell us the delete event is for a
              // directory or give us its inode or fileId.
              event(
                {type: 'delete', path: getPath('dir2/subdir'), kind: 'unknown'},
                {backend},
              ),
              event(
                {
                  type: 'create',
                  path: getPath('dir2/subdir2'),
                  ino: subdirIno,
                  fileId: subdirFileId,
                  kind: subdirKind,
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
          await nextEvent();

          await fs.symlink(f1, f2);
          let {ino, fileId, kind} = await getMetadata(f2);

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'create', path: f2, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a symlink is updated', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await nextEvent();

          await fs.symlink(f1, f2);
          await nextEvent();

          let {ino, fileId, kind} = await getMetadata(f1);
          await fs.writeFile(f2, 'hi');

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'update', path: f1, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a symlink is renamed', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          let f3 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await nextEvent();

          await fs.symlink(f1, f2);
          let {ino, fileId, kind} = await getMetadata(f2);
          await nextEvent();

          await fs.rename(f2, f3);

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'delete', path: f2, ino, fileId, kind}, {backend}),
            event({type: 'create', path: f3, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should emit when a symlink is deleted', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await nextEvent();

          await fs.symlink(f1, f2);
          let {ino, fileId, kind} = await getMetadata(f2);
          await nextEvent();

          fs.unlink(f2);

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'delete', path: f2, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should not crash when a folder symlink is created', async () => {
          let f1 = getFilename();
          let f2 = getFilename();
          await fs.mkdir(f1);
          await nextEvent();

          await fs.symlink(f1, f2);
          let {ino, fileId, kind} = await getMetadata(f2);
          await nextEvent();

          fs.unlink(f2);

          let res = await nextEvent();
          // XXX: winfs.lstatSync tells us f2 is a directory while it is a
          // symlink and should rather be considered a symlink (or a least a
          // file).
          assert.deepEqual(res, [
            event(
              {type: 'delete', path: f2, ino, fileId, kind: 'file'},
              {backend},
            ),
          ]);
        });
      });

      describe('rapid changes', () => {
        it('should coalese create and update events', async () => {
          let f1 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await fs.writeFile(f1, 'updated');
          let {ino, fileId, kind} = await getMetadata(f1);

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'create', path: f1, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should coalese delete and create events into a single update event', async () => {
          if (backend === 'watchman' && process.platform === 'linux') {
            // It seems that watchman on Linux emits a single event
            // when rapidly deleting and creating a file so our event
            // coalescing is not working in that case
            // https://github.com/parcel-bundler/watcher/pull/84#issuecomment-981117725
            // https://github.com/facebook/watchman/issues/980
            return;
          }

          let f1 = getFilename();
          await fs.writeFile(f1, 'hello world');
          await nextEvent();

          await fs.unlink(f1);
          await fs.writeFile(f1, 'hello world again');
          let {ino, fileId, kind} = await getMetadata(f1);

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'update', path: f1, ino, fileId, kind}, {backend}),
          ]);
        });

        if (backend !== 'fs-events') {
          it('should ignore files that are created and deleted rapidly', async () => {
            let f1 = getFilename();
            let f2 = getFilename();
            await fs.writeFile(f1, 'hello world');
            let {ino, fileId, kind} = await getMetadata(f1);
            await fs.writeFile(f2, 'hello world');
            fs.unlink(f2);

            let res = await nextEvent();
            assert.deepEqual(res, [
              event({type: 'create', path: f1, ino, fileId, kind}, {backend}),
            ]);
          });

          it('should coalese create and rename events', async () => {
            let f1 = getFilename();
            let f2 = getFilename();
            await fs.writeFile(f1, 'hello world');
            let {ino, fileId, kind} = await getMetadata(f1);
            await fs.rename(f1, f2);

            let res = await nextEvent();
            assert.deepEqual(res, [
              event({type: 'create', path: f2, ino, fileId, kind}, {backend}),
            ]);
          });

          it('should coalese multiple rename events', async () => {
            let f1 = getFilename();
            let f2 = getFilename();
            let f3 = getFilename();
            let f4 = getFilename();
            await fs.writeFile(f1, 'hello world');
            let {ino, fileId, kind} = await getMetadata(f1);
            await fs.rename(f1, f2);
            await fs.rename(f2, f3);
            await fs.rename(f3, f4);

            let res = await nextEvent();
            assert.deepEqual(res, [
              event({type: 'create', path: f4, ino, fileId, kind}, {backend}),
            ]);
          });
        }

        it('should coalese multiple update events', async () => {
          let f1 = getFilename();
          await fs.writeFile(f1, 'hello world');
          let {ino, fileId, kind} = await getMetadata(f1);
          await nextEvent();

          await fs.writeFile(f1, 'update');
          await fs.writeFile(f1, 'update2');
          await fs.writeFile(f1, 'update3');

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'update', path: f1, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should coalese update and delete events', async () => {
          let f1 = getFilename();
          await fs.writeFile(f1, 'hello world');
          let {ino, fileId, kind} = await getMetadata(f1);
          await nextEvent();

          await fs.writeFile(f1, 'update');
          fs.unlink(f1);

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'delete', path: f1, ino, fileId, kind}, {backend}),
          ]);
        });
      });

      describe('ignore', () => {
        it('should ignore a directory', async () => {
          let f1 = getFilename();
          let f2 = getFilename(path.basename(ignoreDir));
          await fs.mkdir(ignoreDir);

          await fs.writeFile(f1, 'hello');
          let {ino, fileId, kind} = await getMetadata(f1);
          await fs.writeFile(f2, 'sup');

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'create', path: f1, ino, fileId, kind}, {backend}),
          ]);
        });

        it('should ignore a file', async () => {
          let f1 = getFilename();

          await fs.writeFile(f1, 'hello');
          let {ino, fileId, kind} = await getMetadata(f1);
          await fs.writeFile(ignoreFile, 'sup');

          let res = await nextEvent();
          assert.deepEqual(res, [
            event({type: 'create', path: f1, ino, fileId, kind}, {backend}),
          ]);
        });
      });

      describe('multiple', () => {
        it('should support multiple watchers for the same directory', async () => {
          let dir = path.join(
            fs.realpathSync(require('os').tmpdir()),
            Math.random().toString(31).slice(2),
          );
          fs.mkdirpSync(dir);
          await new Promise((resolve) => setTimeout(resolve, 100));

          function listen() {
            return new Promise(async (resolve) => {
              let sub = await watcher.subscribe(
                dir,
                async (err, events) => {
                  setImmediate(async () => {
                    await sub.unsubscribe();

                    resolve(events);
                  });
                },
                {backend},
              );
            });
          }

          let l1 = listen();
          let l2 = listen();
          await new Promise((resolve) => setTimeout(resolve, 100));

          let test1 = path.join(dir, 'test1.txt');
          await fs.writeFile(test1, 'test1');
          let {ino, fileId, kind} = await getMetadata(test1);

          let res = await Promise.all([l1, l2]);
          assert.deepEqual(res, [
            [
              event(
                {type: 'create', path: test1, ino, fileId, kind},
                {backend},
              ),
            ],
            [
              event(
                {type: 'create', path: test1, ino, fileId, kind},
                {backend},
              ),
            ],
          ]);
        });

        it('should support multiple watchers for the same directory with different ignore paths', async () => {
          let dir = path.join(
            fs.realpathSync(require('os').tmpdir()),
            Math.random().toString(31).slice(2),
          );
          fs.mkdirpSync(dir);
          await new Promise((resolve) => setTimeout(resolve, 100));

          function listen(ignore) {
            return new Promise(async (resolve) => {
              let sub = await watcher.subscribe(
                dir,
                async (err, events) => {
                  setImmediate(async () => {
                    await sub.unsubscribe();

                    resolve(events);
                  });
                },
                {backend, ignore},
              );
            });
          }

          let test1 = path.join(dir, 'test1.txt');
          let test2 = path.join(dir, 'test2.txt');
          let l1 = listen([test1]);
          let l2 = listen([test2]);
          await new Promise((resolve) => setTimeout(resolve, 100));

          await fs.writeFile(test1, 'test1');
          let {
            ino: test1Ino,
            fileId: test1FileId,
            kind: test1Kind,
          } = await getMetadata(test1);
          await fs.writeFile(test2, 'test1');
          let {
            ino: test2Ino,
            fileId: test2FileId,
            kind: test2Kind,
          } = await getMetadata(test2);

          let res = await Promise.all([l1, l2]);
          assert.deepEqual(res, [
            [
              event(
                {
                  type: 'create',
                  path: test2,
                  ino: test2Ino,
                  fileId: test2FileId,
                  kind: test2Kind,
                },
                {backend},
              ),
            ],
            [
              event(
                {
                  type: 'create',
                  path: test1,
                  ino: test1Ino,
                  fileId: test1FileId,
                  kind: test1Kind,
                },
                {backend},
              ),
            ],
          ]);
        });

        it('should support multiple watchers for different directories', async () => {
          let dir1 = path.join(
            fs.realpathSync(require('os').tmpdir()),
            Math.random().toString(31).slice(2),
          );
          let dir2 = path.join(
            fs.realpathSync(require('os').tmpdir()),
            Math.random().toString(31).slice(2),
          );
          fs.mkdirpSync(dir1);
          fs.mkdirpSync(dir2);
          await new Promise((resolve) => setTimeout(resolve, 100));

          function listen(dir) {
            return new Promise(async (resolve) => {
              let sub = await watcher.subscribe(
                dir,
                async (err, events) => {
                  setImmediate(async () => {
                    await sub.unsubscribe();

                    resolve(events);
                  });
                },
                {backend},
              );
            });
          }

          let test1 = path.join(dir1, 'test1.txt');
          let test2 = path.join(dir2, 'test1.txt');
          let l1 = listen(dir1);
          let l2 = listen(dir2);
          await new Promise((resolve) => setTimeout(resolve, 100));

          await fs.writeFile(test1, 'test1');
          let {
            ino: test1Ino,
            fileId: test1FileId,
            kind: test1Kind,
          } = await getMetadata(test1);
          await fs.writeFile(test2, 'test1');
          let {
            ino: test2Ino,
            fileId: test2FileId,
            kind: test2Kind,
          } = await getMetadata(test2);

          let res = await Promise.all([l1, l2]);
          assert.deepEqual(res, [
            [
              event(
                {
                  type: 'create',
                  path: test1,
                  ino: test1Ino,
                  fileId: test1FileId,
                  kind: test1Kind,
                },
                {backend},
              ),
            ],
            [
              event(
                {
                  type: 'create',
                  path: test2,
                  ino: test2Ino,
                  fileId: test2FileId,
                  kind: test2Kind,
                },
                {backend},
              ),
            ],
          ]);
        });

        it('should work when getting events since a snapshot on an already watched directory', async () => {
          let dir = path.join(
            fs.realpathSync(require('os').tmpdir()),
            Math.random().toString(31).slice(2),
          );
          let snapshot = path.join(
            fs.realpathSync(require('os').tmpdir()),
            Math.random().toString(31).slice(2),
          );
          fs.mkdirpSync(dir);
          await new Promise((resolve) => setTimeout(resolve, 100));

          function listen(dir) {
            return new Promise(async (resolve) => {
              let sub = await watcher.subscribe(
                dir,
                (err, events) => {
                  setImmediate(() => resolve([events, sub]));
                },
                {backend},
              );
            });
          }

          let test1 = path.join(dir, 'test1.txt');
          let test2 = path.join(dir, 'test2.txt');
          let l = listen(dir);
          await new Promise((resolve) => setTimeout(resolve, 100));

          await fs.writeFile(test1, 'hello1');
          let {
            ino: test1Ino,
            fileId: test1FileId,
            kind: test1Kind,
          } = await getMetadata(test1);
          await new Promise((resolve) => setTimeout(resolve, 100));

          await watcher.writeSnapshot(dir, snapshot, {backend});
          await new Promise((resolve) => setTimeout(resolve, 1000));

          await fs.writeFile(test2, 'hello2');
          let {
            ino: test2Ino,
            fileId: test2FileId,
            kind: test2Kind,
          } = await getMetadata(test2);
          await new Promise((resolve) => setTimeout(resolve, 100));

          let [watched, sub] = await l;
          assert.deepEqual(watched, [
            event(
              {
                type: 'create',
                path: test1,
                ino: test1Ino,
                fileId: test1FileId,
                kind: test1Kind,
              },
              {backend},
            ),
          ]);

          let since = await watcher.getEventsSince(dir, snapshot, {backend});
          assert.deepEqual(since, [
            event(
              {
                type: 'create',
                path: test2,
                ino: test2Ino,
                fileId: test2FileId,
                kind: test2Kind,
              },
              {backend},
            ),
          ]);

          await sub.unsubscribe();
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
            await watcher.subscribe(
              dir,
              (err, events) => {
                assert(false, 'Should not get here');
              },
              {backend},
            );
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
            await watcher.subscribe(
              file,
              (err, events) => {
                assert(false, 'Should not get here');
              },
              {backend},
            );
          } catch (err) {
            threw = true;
          }

          assert(threw, 'did not throw');
        });
      });
    });
  });

  if (backends.includes('watchman')) {
    describe('watchman errors', () => {
      it('should emit an error when watchman dies', async () => {
        let dir = path.join(
          fs.realpathSync(require('os').tmpdir()),
          Math.random().toString(31).slice(2),
        );
        fs.mkdirpSync(dir);
        await new Promise((resolve) => setTimeout(resolve, 100));

        let p = new Promise((resolve) => {
          watcher.subscribe(
            dir,
            (err, events) => {
              setImmediate(() => resolve(err));
            },
            {backend: 'watchman'},
          );
        });

        execSync('watchman shutdown-server');

        let err = await p;
        assert(err, 'No error was emitted');
      });
    });
  }
});
