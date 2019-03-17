{
  "targets": [
    {
      "target_name": "fschanges",
      "sources": [ "src/FSChanges.cc" ],
      "include_dirs" : [
        "<!(node -e \"require('nan')\")"
      ],
      "conditions": [
        ['OS=="mac"', {
          "variables": {
            "use_fts%": "false",
            "use_watchman%": "false"
          },
          "conditions": [
            ['use_fts=="true"', {
              "sources": ["src/shared/brute.cc", "src/unix/fts.cc"]
            }, {
              "conditions": [
                ['use_watchman=="true"', {
                  "sources": ["src/watchman/BSER.cc", "src/watchman/watchman.cc"]
                }, {
                  "sources": ["src/macos/FSEvents.cc"],
                }]
              ]
            }]
          ],
          "link_settings": {
            "libraries": ["CoreServices.framework"]
          }
        }],
        ['OS=="linux"', {
          "sources": ["src/shared/brute.cc", "src/unix/fts.cc"]
        }],
        ['OS=="win"', {
          "sources": ["src/shared/brute.cc", "src/windows/win.cc"]
        }]
      ]
    }
  ]
}
