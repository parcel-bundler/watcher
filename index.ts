import path from 'path';
import micromatch from 'micromatch';
import isGlob from 'is-glob';
import {ChokidarBackend} from './src/chokidar/ChokidarBackend';

export declare type FilePath = string;
export declare type GlobPattern = string;

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
function getBinding(opts?: Options) {
  if (opts?.backend === 'chokidar') {
    return new ChokidarBackend();
  }

  try {
    return (binding ||= require('node-gyp-build')(__dirname));
  } catch {
    return new ChokidarBackend();
  }
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

export async function subscribe(
  dir: FilePath,
  fn: SubscribeCallback,
  opts?: Options,
): Promise<AsyncSubscription> {
  const binding = getBinding(opts);

  dir = path.resolve(dir);
  opts = normalizeOptions(dir, opts);
  await binding.subscribe(dir, fn, opts);

  return {
    unsubscribe() {
      return binding.unsubscribe(dir, fn, opts);
    },
  };
}

export function unsubscribe(
  dir: FilePath,
  fn: SubscribeCallback,
  opts?: Options,
): Promise<void> {
  const binding = getBinding(opts);

  return binding.unsubscribe(
    path.resolve(dir),
    fn,
    normalizeOptions(dir, opts),
  );
}

export function writeSnapshot(
  dir: FilePath,
  snapshot: FilePath,
  opts?: Options,
): Promise<FilePath> {
  const binding = getBinding(opts);

  return binding.writeSnapshot(
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
  const binding = getBinding(opts);

  return binding.getEventsSince(
    path.resolve(dir),
    path.resolve(snapshot),
    normalizeOptions(dir, opts),
  );
}
