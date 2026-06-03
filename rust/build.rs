fn main() {
  let target_os = std::env::var("CARGO_CFG_TARGET_OS").unwrap();

  let mut build = cc::Build::new();
  build
    .cpp(true)
    .flag("-std=c++17")
    .includes(&["../src"])
    .files(&[
      "src/rust_bindings.cc",
      "../src/Watcher.cc",
      "../src/Glob.cc",
      "../src/DirTree.cc",
      "../src/Debounce.cc",
      "../src/Backend.cc",
    ]);

  match target_os.as_str() {
    "macos" => {
      std::env::set_var("MACOSX_DEPLOYMENT_TARGET", "11.0");
      build
        .files(&[
          "../src/watchman/BSER.cc",
          "../src/watchman/WatchmanBackend.cc",
          "../src/shared/BruteForceBackend.cc",
          "../src/unix/fts.cc",
          "../src/macos/FSEventsBackend.cc",
          "../src/kqueue/KqueueBackend.cc",
        ])
        .define("WATCHMAN", None)
        .define("BRUTE_FORCE", None)
        .define("FS_EVENTS", None)
        .define("KQUEUE", None);
      println!("cargo:rustc-link-lib=framework=CoreServices");
    }
    "linux" | "android" => {
      build
        .files(&[
          "../src/watchman/BSER.cc",
          "../src/watchman/WatchmanBackend.cc",
          "../src/shared/BruteForceBackend.cc",
          "../src/linux/InotifyBackend.cc",
          "../src/unix/legacy.cc",
        ])
        .define("WATCHMAN", None)
        .define("INOTIFY", None)
        .define("BRUTE_FORCE", None);
    }
    "windows" => {
      build
        .files(&[
          "../src/watchman/BSER.cc",
          "../src/watchman/WatchmanBackend.cc",
          "../src/shared/BruteForceBackend.cc",
          "../src/windows/WindowsBackend.cc",
          "../src/windows/win_utils.cc",
        ])
        .define("WATCHMAN", None)
        .define("WINDOWS", None)
        .define("BRUTE_FORCE", None);
    }
    "freebsd" => {
      build
        .files(&[
          "../src/watchman/BSER.cc",
          "../src/watchman/WatchmanBackend.cc",
          "../src/shared/BruteForceBackend.cc",
          "../src/unix/fts.cc",
          "../src/kqueue/KqueueBackend.cc",
        ])
        .define("WATCHMAN", None)
        .define("BRUTE_FORCE", None)
        .define("KQUEUE", None);
    }
    os => panic!("Unsupported target OS: {}", os),
  }

  build.compile("libwatcher.a");
  println!("cargo:rustc-link-lib=static=watcher");
}
