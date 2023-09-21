'use strict';
const nullthrows = require('nullthrows');
const assert = require('assert');
const path = require('path');
const fs = require('fs/promises');
const simpleGit = require('simple-git');

function getHEADCommit(git) {
  return git.revparse('HEAD');
}

// async function getChangedFiles(git) {
//   console.log(await git.diffSummary())
//   return (await git.diffSummary()).files.map((f) => f.file);
// }

// async function getUntrackedFiles(git) {
//   return (await git.raw('ls-files', '--others', '--exclude-standard'))
//     .trim()
//     .split('\n');
// }

async function getEventsBetweenCommits(git, base, ref) {
  return (await git.diff([base, ref, '--name-status']))
    .split('\n')
    .flatMap((l) => {
      if (!l) return [];

      let [status, f, f2] = l.split('\t');
      if (status === 'A') return [[f, 'create']];
      else if (status === 'M') return [[f, 'update']];
      else if (status === 'D') return [[f, 'delete']];
      else if (status[0] === 'R') {
        return [
          [f, 'delete'],
          [f2, 'create'],
        ];
      } else {
        throw new Error(
          `Unknown status diff ${base} ${ref} --name-status: ${status} ${f} ${
            f2 ?? ''
          }`,
        );
      }
    });
}

async function getLocalChanges(git, root) {
  let status = await git.status();

  let localChanges = new Map();
  let localChangesMtime = new Map();

  for (let f of [...status.not_added, ...status.created]) {
    localChanges.set(f, 'create');
    localChangesMtime.set(
      f,
      (await fs.stat(path.join(root, f))).mtime.getTime(),
    );
  }
  for (let f of status.deleted) {
    localChanges.set(f, 'delete');
  }
  for (let f of status.modified) {
    localChanges.set(f, 'update');
    localChangesMtime.set(
      f,
      (await fs.stat(path.join(root, f))).mtime.getTime(),
    );
  }

  return {localChanges, localChangesMtime};
}

module.exports.writeSnapshot = async function writeSnapshot(
  dir,
  snapshotPath,
  opts,
) {
  let git = simpleGit({baseDir: dir});
  let commit = await getHEADCommit(git);

  let {localChanges, localChangesMtime} = await getLocalChanges(git, dir);

  await fs.writeFile(
    snapshotPath,
    JSON.stringify(
      {
        commit,
        localChanges: [...localChanges],
        localChangesMtime: [...localChangesMtime],
      },
      null,
      2,
    ),
  );
};

module.exports.getEventsSince = async function getEventsSince(
  dir,
  snapshotPath,
  opts,
) {
  const git = simpleGit({baseDir: dir});

  let snapshotData = opts.remoteSnapshot;
  if (!snapshotData) {
    try {
      snapshotData = JSON.parse(await fs.readFile(snapshotPath, 'utf8'));
    } catch (e) {
      return [];
    }
  }

  let oldCommit = snapshotData.commit;
  let commit = await getHEADCommit(git);

  let eventsCommited = new Map(
    await getEventsBetweenCommits(git, oldCommit, commit),
  );

  let oldLocalChanges = new Map(snapshotData.localChanges);
  let oldLocalChangesMtime = new Map(snapshotData.localChangesMtime);
  let {localChanges, localChangesMtime} = await getLocalChanges(git, dir);

  // console.log({
  //   eventsCommited,
  //   oldLocalChanges,
  //   oldLocalChangesMtime,
  //   localChanges,
  //   localChangesMtime,
  // });

  // Now added and..
  //   ...previously added and not listed in commited changes: compare mtime && >changed
  //   ...previously added and in commited changes: >changed
  //   ...previously changed and (therefore deleted in commited changes): >added
  //   ...previously deleted and (therefore deleted in commited changes): >added
  //   ...no previous local change: >added
  // Now changed and..
  //   ...previously added and (therefore listed in commited changes): >changed
  //   ...previously changed and not listed in commited changes: compare mtime && >changed
  //   ...previously changed and listed in commited changes: >changed
  //   ...previously deleted and (therefore added in commited changes): >added
  //   ...(no previous local change and added in commited changes: >added)
  //   ...(no previous local change and changed in commited changes: >changed)
  //   ...(no previous local change and deleted in commited changes: impossible)
  //   ...no previous local change and not listed in commited changes: >changed
  // Now deleted and..
  //   ...previously added: >deleted
  //   ...previously changed: >deleted
  //   ...previously deleted: nothing
  //   ...no previous local change (and therefore added in commited changes): >delete
  //  previously deleted and..
  //   ...now added/changed/no local change: >added
  let events = new Map(eventsCommited);
  for (let [path, type] of localChanges) {
    let oldType = oldLocalChanges.get(path);
    if (type === 'create') {
      if (oldType === 'create') {
        if (!eventsCommited.has(path)) {
          if (localChangesMtime.get(path) > oldLocalChangesMtime.get(path)) {
            events.set(path, 'update');
          }
        } else {
          events.set(path, 'update');
        }
      } /*  if (oldType === 'update' || oldType === 'delete' || oldType == null)  */ else {
        events.set(path, 'create');
      }
    } else if (type === 'update') {
      if (oldType === 'create') {
        events.set(path, 'update');
      } else if (oldType === 'update') {
        if (!eventsCommited.has(path)) {
          if (localChangesMtime.get(path) > oldLocalChangesMtime.get(path)) {
            events.set(path, 'update');
          }
        } else {
          events.set(path, 'update');
        }
      } else if (oldType === 'delete') {
      } /* if (oldType == null) */ else {
        events.set(path, 'update');
      }
    } else if (type === 'delete') {
      if (oldType === 'create' || oldType === 'update') {
        events.set(path, 'delete');
        // } else if (oldType === 'delete') {
      } else if (oldType == null) {
        events.set(path, 'delete');
      }
    }
  }
  for (let {path, type} of oldLocalChanges) {
    if (type === 'delete' && localChanges.get(f) !== 'delete') {
      events.set(f, 'delete');
    }
  }
  // console.log(events);
  return [...events].map(([f, type]) => ({type, path: f}));
};
