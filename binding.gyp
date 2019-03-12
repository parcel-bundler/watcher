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
          "sources": ["src/macos/FSEvents.cc"],
          "link_settings": {
            "libraries": ["CoreServices.framework"]
          }
        }]
      ]
    }
  ]
}
