import path from 'path';
import micromatch from 'micromatch';
import isGlob from 'is-glob';

declare type FilePath = string;
declare type GlobPattern = string;

export declare namespace ParcelWatcher {
  export {
    BackendType,
    EventType,
    Options,
    SubscribeCallback,
    AsyncSubscription,
    Event,
    getEventsSince,
    subscribe,
    unsubscribe,
    writeSnapshot,
  };
}

export type BackendType =
  | 'fs-events'
  | 'watchman'
  | 'inotify'
  | 'windows'
  | 'brute-force'
  | 'chokidar';

export type EventType = 'create' | 'update' | 'delete';

export interface Options {
  ignore?: (FilePath | GlobPattern)[];
  backend?: BackendType;
}

export type SubscribeCallback = (err: Error | null, events: Event[]) => unknown;

export interface AsyncSubscription {
  unsubscribe(): Promise<void>;
}

export interface Event {
  path: FilePath;
  type: EventType;
}

let binding: any;
function getBinding() {
  return (binding ||= require('node-gyp-build')(__dirname));
}

function normalizeOptions(
  dir: string,
  opts: Record<string, any> & Options = {},
) {
  const {ignore, ...rest} = opts;

  if (Array.isArray(ignore)) {
    opts = {...rest};

    for (const value of ignore) {
      if (isGlob(value)) {
        if (!opts.ignoreGlobs) {
          opts.ignoreGlobs = [];
        }

        const regex = micromatch.makeRe(value, {
          // We set `dot: true` to workaround an issue with the
          // regular expression on Linux where the resulting
          // negative lookahead `(?!(\\/|^)` was never matching
          // in some cases. See also https://bit.ly/3UZlQDm
          dot: true,
          // C++ does not support lookbehind regex patterns, they
          // were only added later to JavaScript engines
          // (https://bit.ly/3V7S6UL)
          lookbehinds: false,
        });
        opts.ignoreGlobs.push(regex.source);
      } else {
        if (!opts.ignorePaths) {
          opts.ignorePaths = [];
        }

        opts.ignorePaths.push(path.resolve(dir, value));
      }
    }
  }

  return opts;
}

export function writeSnapshot(
  dir: FilePath,
  snapshot: FilePath,
  opts?: Options,
): Promise<FilePath> {
  return getBinding().writeSnapshot(
    path.resolve(dir),
    path.resolve(snapshot),
    normalizeOptions(dir, opts),
  );
}

export function getEventsSince(
  dir: FilePath,
  snapshot: FilePath,
  opts?: Options,
): Promise<Event[]> {
  return getBinding().getEventsSince(
    path.resolve(dir),
    path.resolve(snapshot),
    normalizeOptions(dir, opts),
  );
}

export async function subscribe(
  dir: FilePath,
  fn: SubscribeCallback,
  opts?: Options,
): Promise<AsyncSubscription> {
  dir = path.resolve(dir);
  opts = normalizeOptions(dir, opts);
  await getBinding().subscribe(dir, fn, opts);

  return {
    unsubscribe() {
      return getBinding().unsubscribe(dir, fn, opts);
    },
  };
}

export function unsubscribe(
  dir: FilePath,
  fn: SubscribeCallback,
  opts?: Options,
): Promise<void> {
  return getBinding().unsubscribe(
    path.resolve(dir),
    fn,
    normalizeOptions(dir, opts),
  );
}
