https://github.com/facebook/watchman/pull/934

https://facebook.github.io/watchman/docs/scm-query

## no mtime clutter through branch switching

1. checkout branch x
2. getEventsSince() -> empty/all
3. checkout branch y (changes some files)
4. checkout branch x again
5. getEventsSince() -> no changes (+ everything uncommited)
