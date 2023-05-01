{
  "targets": [{
    "target_name": "ad-block",
    "sources": [
      "addon.cc",
      "ad_block_client_wrap.cc",
      "ad_block_client_wrap.h",
      "ad_block_client.cc",
      "ad_block_client.h",
      "context_domain.cc",
      "context_domain.h",
      "cosmetic_filter.cc",
      "cosmetic_filter.h",
      "filter.cc",
      "filter.h",
      "filter_list.cc",
      "filter_list.h",
      "no_fingerprint_domain.cc",
      "no_fingerprint_domain.h",
      "protocol.cc",
      "protocol.h",
      "./node_modules/bloom-filter-cpp/BloomFilter.cpp",
      "./node_modules/bloom-filter-cpp/BloomFilter.h",
      "./node_modules/bloom-filter-cpp/hashFn.cpp",
      "./node_modules/bloom-filter-cpp/hashFn.h",
      "./node_modules/hashset-cpp/hash_set.cc",
      "./node_modules/hashset-cpp/hash_set.h"
    ],
    "include_dirs": [
      ".",
      './node_modules/bloom-filter-cpp',
      './node_modules/hashset-cpp'
    ],
    "dependencies": [
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
