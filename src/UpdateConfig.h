/*
Copyright (C) 2023- YAYC team <info@yayc.stream>

This work is licensed under the terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/ or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

In addition to the above,
- The use of this work for training, fine-tuning, or otherwise feeding artificial intelligence systems is prohibited for both commercial and non-commercial use.
  This includes, but is not limited to, the ingestion of this work into large language models (LLMs), code generation models,
  Retrieval-Augmented Generation (RAG) systems, embedding databases, vector stores, or any other AI-assisted system.
- Any and all donation options in derivative work must be the same as in the original work.
- All use of this work outside of the above terms must be explicitly agreed upon in advance with the exclusive copyright owner(s).
- Any derivative work must retain the above copyright and acknowledge that any and all use of the derivative work outside the above terms
  must be explicitly agreed upon in advance with the exclusive copyright owner(s) of the original work.

*/

#ifndef UPDATECONFIG_H
#define UPDATECONFIG_H

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

// Everything the updater reads from the machine: install type, update method,
// release URLs, paths. detect() is production; tests fill in their own
struct UpdateConfig
{
    Q_GADGET
public:
    enum InstallType {
        Unknown = 0,
        SelfBuilt,        // no YAYC_DIST_BUILD define: never auto-update
        LinuxAppImage,
        LinuxTarball,     // manual extract, anywhere
        LinuxDeb,         // binary listed by dpkg (the deb installs to /opt/yayc)
        WindowsPortable,
        WindowsInstalled, // Inno Setup (unins000.exe next to the binary)
        MacOSBundle
    };
    Q_ENUM(InstallType)

    // How an update gets installed. NoMethod: updates unsupported
    enum Method {
        NoMethod = 0,
        ReplaceAppImage,  // staged .new file, renamed at exit
        SwapDirectory,    // staged tree, swapped in at exit (tarball)
        ElevatedDpkg,     // pkexec dpkg -i, then restart
        ManualDeb,        // the user runs the shown sudo dpkg command
        HelperSwap,       // Windows portable: staged tree, swapped by a helper after
                          // exit (a running process locks its DLLs)
        Installer,        // the helper runs the installer after exit (UAC)
        OpenDmg           // macOS: opens the downloaded dmg
    };
    Q_ENUM(Method)

    InstallType installType{Unknown};
    Method updateMethod{NoMethod};
    QString unsupportedReason;
    // <repository>/releases/download. Feed URLs under it may be plain http (test servers)
    QString releasesUrl;
    QString latestVersionUrl;      // the APPVERSION file
    QString updateFeedUrl;         // latest.json: package URLs and hashes
    QString currentVersion;        // of the running build
    // Of this binary, not the CPU (x64 may run emulated on ARM). Qt names:
    // "x86_64" or "arm64"; anything else has no release artifact
    QString arch;
    QString exePath;               // live executable
    QString installRoot;           // its directory (tarball / portable root)
    QString appImagePath;          // runningAppImage(); Linux AppImage only
    QString updatesDir;            // <cache>/updates/<key of this install>
    // ElevatedDpkg: by path from a root-owned dir. A user's PATH may hold a
    // fake one that phishes the password
    QString pkexecPath;
    // What the helper's relaunch passes on (main.cpp: -c already absolute)
    QStringList relaunchArgs;
    // Files of the running install (:/yayc-manifest.txt), as manifestKey()s.
    // Empty: unknown, so directory swaps are unsupported
    QSet<QString> shippedFiles;
    int transferTimeoutMs{30000};  // no byte for this long: abort
    int cleanupDelayMs{60000};     // < 0: no startup cleanup
    // A verify this recent counts as fresh: feed and checksums not fetched again
    qint64 freshVerifyMs{10 * 60 * 1000};

    static UpdateConfig detect();

    // Release asset of \a version, e.g. "yayc-1.9.0-x86_64.AppImage". Empty:
    // this install type has none
    QString assetName(const QString &version) const;

    // The new tree is extracted next to the install, then swapped in
    static bool isDirSwap(Method m);
    // Root or admin runs the install: it needs the hash checked this session
    static bool isElevated(Method m);
    // A line of dpkg's per-package file list equals \a filePath
    static bool isListedByDpkg(const QString &listFile, const QString &filePath);
    // Directory swaps only for a dir named after YAYC: the swap moves the
    // whole dir, and cleanup deletes it later
    static bool isYaycDir(const QString &dir);
    // $APPIMAGE of this process, or empty. Children inherit the variable, so
    // it counts only when this executable runs from inside $APPDIR
    static QString runningAppImage();
    static QString runningAppImage(const QString &appImage, const QString &appDir,
                                   const QString &exePath);
    // <cacheRoot>/updates/<key of \a install>. AppImage: the .AppImage file,
    // not installRoot (a new mount at every start)
    static QString updatesDirFor(const QString &cacheRoot, const QString &install);

private:
    // From the exe, AppImage and arch already in \a c
    static InstallType detectInstallType(const UpdateConfig &c);
    static void computeUpdateMethod(UpdateConfig &c);
    static bool probeDirWritable(const QString &path);
};

#endif // UPDATECONFIG_H
