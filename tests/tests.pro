QT += testlib
QT += quick core gui network widgets
QT += quickcontrols2
QT += webchannel qml
QT += webenginecore webenginewidgets webenginequick webenginequickdelegatesqml
QT += core5compat

CONFIG += c++20 console
CONFIG -= app_bundle

APPVERSION = "$$cat($$PWD/../APPVERSION)"
DEFINES += "APPVERSION=\"$$APPVERSION\""

INCLUDEPATH += ../src

# The test-build hooks are tested too (TestControl)
DEFINES += YAYC_TEST_BUILD

SOURCES += tst_yayc.cpp \
           ../src/Platform.cpp \
           ../src/VideoMetadata.cpp \
           ../src/ChannelMetadata.cpp \
           ../src/NoDirSortProxyModel.cpp \
           ../src/FileSystemModel.cpp \
           ../src/ThumbnailFetcher.cpp \
           ../src/RequestInterceptor.cpp \
           ../src/YaycUtilities.cpp \
           ../src/UpdateManager.cpp \
           ../src/UpdateFetcher.cpp \
           ../src/UpdateConfig.cpp \
           ../src/UpdateInstall.cpp \
           ../src/UpdateWindows.cpp \
           ../src/UpdateLinux.cpp \
           ../src/SingleInstance.cpp \
           ../src/TestControl.cpp

HEADERS += ../src/Platform.h \
           ../src/VideoMetadata.h \
           ../src/ChannelMetadata.h \
           ../src/ThumbnailImageProvider.h \
           ../src/EmptyIconProvider.h \
           ../src/NoDirSortProxyModel.h \
           ../src/FileSystemModel.h \
           ../src/ThumbnailFetcher.h \
           ../src/RequestInterceptor.h \
           ../src/YaycUtilities.h \
           ../src/UpdateManager.h \
           ../src/UpdateFetcher.h \
           ../src/UpdateConfig.h \
           ../src/UpdateInstall.h \
           ../src/UpdateWindows.h \
           ../src/UpdateLinux.h \
           ../src/SingleInstance.h \
           ../src/TestControl.h \
           TestHttpServer.h
