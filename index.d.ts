declare type FilePath = string;

declare namespace ParcelWatcher {
  export type WatcherBackendType = 
    | 'fs-events'
    | 'watchman'
    | 'inotify'
    | 'windows'
    | 'brute-force';
  export type WatchEventType = 'create' | 'update' | 'delete';
  export interface WatcherOptions {
    ignore?: FilePath[];
    backend?: WatcherBackendType;
  }
  export type SubscribeCallback = (
    err: Error | null,
    events: WatcherEvent[]
  ) => unknown;
  export interface AsyncSubscription {
    unsubscribe(): Promise<void>;
  }
  export interface WatcherEvent {
    path: FilePath;
    type: WatchEventType;
  }
  export function getEventsSince(
    dir: FilePath,
    snapshot: FilePath,
    opts?: WatcherOptions
  ): Promise<WatcherEvent[]>;
  export function subscribe(
    dir: FilePath,
    fn: SubscribeCallback,
    opts?: WatcherOptions
  ): Promise<AsyncSubscription>;
  export function unsubscribe(
    dir: FilePath,
    fn: SubscribeCallback,
    opts?: WatcherOptions
  ): Promise<void>;
  export function writeSnapshot(
    dir: FilePath,
    snapshot: FilePath,
    opts?: WatcherOptions
  ): Promise<FilePath>;
}

export = ParcelWatcher;