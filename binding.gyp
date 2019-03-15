{
  "targets": [
    {
      "target_name": "fschanges",
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
      "sources": [ "src/FSChanges.cc" ],
      "include_dirs" : ["<!@(node -p \"require('node-addon-api').include\")"],
      "dependencies": ["<!(node -p \"require('node-addon-api').gyp\")"],
      "conditions": [
        ['OS=="mac"', {
          "variables": {
            "use_fts%": "false"
          },
          "conditions": [
            ['use_fts=="true"', {
              "sources": ["src/shared/brute.cc", "src/unix/fts.cc"]
            }, {
              "sources": ["src/macos/FSEvents.cc"],
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
