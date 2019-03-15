{
  "targets": [
    {
      "target_name": "fschanges",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "xcode_settings": { "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "10.7",
      },
      "msvs_settings": {
        "VCCLCompilerTool": { "ExceptionHandling": 1 },
      },
      "sources": [ "src/FSChanges.cc" ],
      "include_dirs" : ["<!@(node -p \"require('node-addon-api').include\")"],
      'dependencies': ["<!(node -p \"require('node-addon-api').gyp\")"],
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
