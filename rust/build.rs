fn main() {
  std::env::set_var("MACOSX_DEPLOYMENT_TARGET", "11.0");

  cc::Build::new()
    .cpp(true) // Enable C++ compilation
    .flag("-std=c++17")
    .includes(&["../src"])
    .define("FS_EVENTS", "true")
    .files(&[
      "src/rust_bindings.cc",
      "../src/Watcher.cc",
      "../src/Glob.cc",
      "../src/DirTree.cc",
      "../src/Debounce.cc",
      "../src/Backend.cc",
      "../src/watchman/BSER.cc",
      "../src/watchman/WatchmanBackend.cc",
      "../src/shared/BruteForceBackend.cc",
      "../src/unix/fts.cc",
      "../src/macos/FSEventsBackend.cc",
      "../src/kqueue/KqueueBackend.cc",
    ]) // Specify your C++ source file
    .compile("libwatcher.a"); // Compile into a static library

  println!("cargo:rustc-link-lib=framework=CoreServices");
  println!("cargo:rustc-link-lib=static=watcher");
}
