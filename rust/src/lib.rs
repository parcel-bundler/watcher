use std::{
  ffi::{c_char, c_int, c_void, CStr, CString, OsStr},
  os::unix::ffi::OsStrExt,
  path::{Path, PathBuf},
  sync::mpsc::{self, Receiver, Sender},
};

type WatcherCb =
  extern "C" fn(data: *const c_void, error: *const c_char, events: *const c_void, len: c_int);

extern "C" {
  fn parcel_watcher_subscribe(dir: *const c_char, cb: WatcherCb, data: *const c_void) -> bool;
  fn parcel_watcher_event_get_path(event: *const c_void, index: c_int) -> *const c_char;
  fn parcel_watcher_event_get_type(event: *const c_void, index: c_int) -> c_int;
  fn parcel_watcher_unsubscribe(dir: *const c_char, cb: WatcherCb, data: *const c_void) -> bool;
}

#[derive(Debug)]
pub enum EventType {
  Created,
  Updated,
  Deleted,
}

#[derive(Debug)]
pub struct Event {
  pub path: PathBuf,
  pub ty: EventType,
}

struct WatcherData {
  tx: Sender<Result<Vec<Event>, String>>,
}

pub struct Watcher {
  dir: CString,
  data: *mut WatcherData,
  rx: Receiver<Result<Vec<Event>, String>>,
}

pub fn watch(dir: &Path) -> Watcher {
  let c_dir = CString::new(dir.as_os_str().as_encoded_bytes()).unwrap();
  let (tx, rx) = mpsc::channel();
  let data = Box::leak(Box::new(WatcherData { tx })) as *mut WatcherData;
  unsafe {
    parcel_watcher_subscribe(c_dir.as_ptr(), watch_cb, data as *const c_void);
  }
  Watcher {
    dir: c_dir,
    data,
    rx,
  }
}

impl Watcher {
  pub fn recv(&self) -> Result<Vec<Event>, String> {
    self.rx.recv().unwrap()
  }
}

impl Drop for Watcher {
  fn drop(&mut self) {
    unsafe {
      parcel_watcher_unsubscribe(self.dir.as_ptr(), watch_cb, self.data as *const c_void);
      drop(Box::from_raw(self.data));
    }
  }
}

extern "C" fn watch_cb(
  data: *const c_void,
  error: *const c_char,
  events: *const c_void,
  len: c_int,
) {
  let data = unsafe { &*(data as *const WatcherData) };
  if !error.is_null() {
    let error = unsafe { CStr::from_ptr(error) };
    data
      .tx
      .send(Err(error.to_str().unwrap().to_owned()))
      .unwrap();
  } else {
    let mut rust_events = Vec::new();
    for i in 0..len {
      let path = unsafe { CStr::from_ptr(parcel_watcher_event_get_path(events, i)) };
      let ty = unsafe { parcel_watcher_event_get_type(events, i) };
      rust_events.push(Event {
        path: PathBuf::from(OsStr::from_bytes(path.to_bytes())),
        ty: if ty == 0 {
          EventType::Created
        } else if ty == 1 {
          EventType::Deleted
        } else {
          EventType::Updated
        },
      });
    }
    data.tx.send(Ok(rust_events)).unwrap();
  }
}
