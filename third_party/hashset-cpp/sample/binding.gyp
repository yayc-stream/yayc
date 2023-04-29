{
  "targets": [{
    "target_name": "sample",
    "type": "executable",
    "sources": [
      "../main.cc",
      "../hash_set.cc",
      "../hash_set.h",
      "../hashFn.cc",
      "../hashFn.h",
      "../test/example_data.h",
    ],
    "include_dirs": [
      "..",
    ],
    "conditions": [
      ['OS=="win"', {
        }, {
          'cflags_cc': [ '-fexceptions' ]
        }
      ]
    ],
    "xcode_settings": {
      "OTHER_CFLAGS": [ "-ObjC" ],
      "OTHER_CPLUSPLUSFLAGS" : ["-std=c++11","-stdlib=libc++", "-v"],
      "OTHER_LDFLAGS": ["-stdlib=libc++"],
      "MACOSX_DEPLOYMENT_TARGET": "10.9",
      "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
      "ARCHS": ["x86_64"],
    },
  }]
}
