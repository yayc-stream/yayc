QT += quick core gui network widgets quickcontrols2
QT += webchannel qml
QT += webengine

CONFIG += c++11
CONFIG += qtquickcompiler
#unix:!android: CONFIG += use_lld_linker # fix for QTBUG-80964

#No debug output in release mode
CONFIG(release): DEFINES += QT_NO_DEBUG_OUTPUT

SOURCES += \
        src/main.cpp

qml.files = $$files($$PWD/src/*.qml)
qml.base = $$PWD/src/
qml.prefix = /

RESOURCES += qml

images.files =  $$files($$PWD/assets/images/*.png) \
                $$files($$PWD/assets/images/*.svg) \
                $$files($$PWD/assets/images/*.webp) \
                $$files($$PWD/assets/images/*.gif)
images.base = $$PWD/assets/
images.prefix = /

RESOURCES += images

doc.files =  $$files($$PWD/docs/img/*.png)
doc.base = $$PWD/docs/img/
doc.prefix = /doc

RESOURCES += doc

fonts.files = $$files($$PWD/assets/fonts/*.*)
fonts.base = $$PWD/assets/
fonts.prefix = /

RESOURCES += fonts

# ToDo: merge with images
icons.files = $$files($$PWD/assets/icons/*.*)
icons.base = $$PWD/assets/
icons.prefix = /

RESOURCES += icons

changelog.files = $$PWD/CHANGELOG.md
changelog.base = $$PWD/
changelog.prefix = /

RESOURCES += changelog

disclaimer.files = $$PWD/DISCLAIMER.md
disclaimer.base = $$PWD/
disclaimer.prefix = /

RESOURCES += disclaimer

OTHER_FILES += LICENSE README.md CHANGELOG.md APPVERSION DONATE DISCLAIMER.md \
               $$files($$PWD/docs/*, true)

APPVERSION = "$$cat($$PWD/APPVERSION)"

DEFINES += "APPVERSION=\"$$APPVERSION\""

SOURCES +=  src/third_party/ad-block/ad_block_client.cc \
            src/third_party/ad-block/no_fingerprint_domain.cc \
            src/third_party/ad-block/filter.cc \
            src/third_party/ad-block/protocol.cc \
            src/third_party/ad-block/context_domain.cc \
            src/third_party/ad-block/cosmetic_filter.cc \
            src/third_party/bloom-filter-cpp/BloomFilter.cpp \
            src/third_party/hashset-cpp/hash_set.cc \
            src/third_party/hashset-cpp/hashFn.cc

HEADERS = src/third_party/ad-block/ad_block_client.h
