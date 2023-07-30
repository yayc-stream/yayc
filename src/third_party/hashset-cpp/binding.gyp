{
  "targets": [{
    "target_name": "hashset-cpp",
    "sources": [
      "addon.cc",
      "hash_set_wrap.cc",
      "hash_set_wrap.h",
      "hash_set.cc",
      "hash_set.h",
      "hashFn.cc",
      "hashFn.h",
    ],
    "include_dirs": [
      ".",
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
      "MACOSX_DEPLOYMENT_TARGET": "10.9",
      "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
    },
  }]
}
