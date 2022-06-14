const watcher = require('../');
const assert = require('assert');
const fs = require('fs-extra');
const path = require('path');

let winfs;
if (process.platform === 'win32') {
  winfs = require('@gyselroth/windows-fsstat');
}

const tmpDir = path.join(
  fs.realpathSync(require('os').tmpdir()),
  Math.random().toString(31).slice(2),
);

let backends = [];
// FIXME: Watchman and FSEvents are not supported yet
if (process.platform === 'darwin') {
  backends = [];
} else if (process.platform === 'linux') {
  backends = ['inotify'];
} else if (process.platform === 'win32') {
  backends = ['windows'];
}

let c = 0;
const getFilename = (...dir) =>
  path.join(tmpDir, ...dir, `test${c++}${Math.random().toString(31).slice(2)}`);

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

backends.forEach((backend) => {
  describe(backend, () => {
    describe('scan', () => {
      beforeEach(async () => {
        await fs.rm(tmpDir, {recursive: true, force: true});
        await fs.mkdirpSync(tmpDir);
      });

      it('should not emit for the scanned directory itself', async () => {
        let res = await watcher.scan(tmpDir, {backend});
        assert.deepEqual(res, []);
      });

      it('should emit when a file is found', async () => {
        let f = getFilename();
        await fs.writeFile(f, 'test');
        let {ino, fileId, kind} = await getMetadata(f);

        let res = await watcher.scan(tmpDir, {backend});
        assert.deepEqual(res, [
          event({type: 'create', path: f, ino, fileId, kind}, {backend}),
        ]);
      });

      it('should emit when a directory is found', async () => {
        let dir = getFilename();
        await fs.mkdir(dir);
        let {ino, fileId, kind} = await getMetadata(dir);

        let res = await watcher.scan(tmpDir, {backend});
        assert.deepEqual(res, [
          event({type: 'create', path: dir, ino, fileId, kind}, {backend}),
        ]);
      });

      it('should emit for sub-directories content', async () => {
        let dir = getFilename();
        let subdir = getFilename(path.basename(dir));
        let file = getFilename(path.basename(dir), path.basename(subdir));
        await fs.mkdir(dir);
        await fs.mkdir(subdir);
        await fs.writeFile(file, 'test');
        let {
          ino: dirIno,
          fileId: dirFileId,
          kind: dirKind,
        } = await getMetadata(dir);
        let {
          ino: subdirIno,
          fileId: subdirFileId,
          kind: subdirKind,
        } = await getMetadata(subdir);
        let {
          ino: fileIno,
          fileId: fileFileId,
          kind: fileKind,
        } = await getMetadata(file);

        let res = await watcher.scan(tmpDir, {backend});
        assert.deepEqual(res, [
          event(
            {
              type: 'create',
              path: dir,
              ino: dirIno,
              fileId: dirFileId,
              kind: dirKind,
            },
            {backend},
          ),
          event(
            {
              type: 'create',
              path: subdir,
              ino: subdirIno,
              fileId: subdirFileId,
              kind: subdirKind,
            },
            {backend},
          ),
          event(
            {
              type: 'create',
              path: file,
              ino: fileIno,
              fileId: fileFileId,
              kind: fileKind,
            },
            {backend},
          ),
        ]);
      });
    });
  });
});
