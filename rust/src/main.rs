use std::path::Path;

use parcel_watcher::watch;

fn main() {
  println!("MAIN!");
  let watcher = watch(Path::new("/Users/devongovett/dev/watcher"));
  while let Ok(events) = watcher.recv() {
    println!("{:?}", events);
    break;
  }

  drop(watcher);
}
