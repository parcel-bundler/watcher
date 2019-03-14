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
            "use_nftw%": "false"
          },
          "conditions": [
            ['use_nftw=="true"', {
              "sources": ["src/shared/brute.cc", "src/unix/nftw.cc"]
            }, {
              "sources": ["src/macos/FSEvents.cc"],
            }]
          ],
          "link_settings": {
            "libraries": ["CoreServices.framework"]
          }
        }],
        ['OS=="linux"', {
          "sources": ["src/shared/brute.cc", "src/unix/nftw.cc"]
        }],
        ['OS=="win"', {
          "sources": ["src/shared/brute.cc", "src/windows/win.cc"]
        }]
      ]
    }
  ]
}
