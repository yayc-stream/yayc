QT += testlib
QT += quick core gui network widgets
QT += quickcontrols2
QT += webchannel qml
QT += webenginecore webenginewidgets webenginequick webenginequickdelegatesqml
QT += core5compat

CONFIG += c++17 console
CONFIG -= app_bundle

APPVERSION = "$$cat($$PWD/../APPVERSION)"
DEFINES += "APPVERSION=\"$$APPVERSION\""

INCLUDEPATH += ../src

SOURCES += tst_yayc.cpp \
           ../src/Platform.cpp \
           ../src/VideoMetadata.cpp \
           ../src/ChannelMetadata.cpp \
           ../src/NoDirSortProxyModel.cpp \
           ../src/FileSystemModel.cpp \
           ../src/ThumbnailFetcher.cpp \
           ../src/RequestInterceptor.cpp \
           ../src/YaycUtilities.cpp

HEADERS += ../src/Platform.h \
           ../src/VideoMetadata.h \
           ../src/ChannelMetadata.h \
           ../src/ThumbnailImageProvider.h \
           ../src/EmptyIconProvider.h \
           ../src/NoDirSortProxyModel.h \
           ../src/FileSystemModel.h \
           ../src/ThumbnailFetcher.h \
           ../src/RequestInterceptor.h \
           ../src/YaycUtilities.h
