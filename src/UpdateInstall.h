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

#ifndef UPDATEINSTALL_H
#define UPDATEINSTALL_H

#include "UpdateConfig.h"

#include <QByteArray>
#include <QLatin1StringView>
#include <QSet>
#include <QString>
#include <QStringList>

#include <atomic>
#include <cstdio>
#include <memory>

class QLockFile;

// Installs a staged update once YAYC is gone: commit, user files carried over,
// the Windows helper. No QObject: main.cpp runs it after the event loop
namespace UpdateInstall {

// In updates/: an install that failed at exit, shown by the next session
inline const QString installErrorName{QStringLiteral("last-install-error.json")};
// In updates/: the Windows helper; it deletes itself once done
inline const QString helperScriptName{QStringLiteral("yayc-update.cmd")};
// Next to it: "<pid> <start token>" of the cmd that runs it
inline const QString helperPidName{QStringLiteral("yayc-update.pid")};
// In updates/: <pid>.lock of each running YAYC of this install
inline const QString runningDirName{QStringLiteral("running")};
// In updates/: the version of a complete directory stage (resumeStage())
inline const QString stageMarkerName{QStringLiteral("staged-version")};
// In updates/: the Windows swap helper's step (aside, in), for a resume
inline const QString journalName{QStringLiteral("swap-journal")};
// In updates/: a YAYC started while the helper ran; the helper starts it
inline const QString relaunchRequestName{QStringLiteral("relaunch-request")};
// Inside a .old the updater made (next to it for an AppImage, a file),
// written before the .old exists. A .old without it is the user's (e.g. a
// hand-made backup): never deleted
inline const QString backupMarkerName{QStringLiteral(".yayc-backup")};
// In updates/: the last version that ran 5 minutes with its window. Until
// the running one has, the .old of its predecessor stays
inline const QString goodStartName{QStringLiteral("good-start")};
// Next to the install: the previous version; the new one (AppImage: staged;
// dir swap: kept after a failed rollback)
inline constexpr QLatin1StringView oldSuffix{".old"};
inline constexpr QLatin1StringView newSuffix{".new"};
// Built into release binaries only (a file on disk could be edited): the
// files of the package, see readManifest()
inline const QString manifestResource{QStringLiteral(":/yayc-manifest.txt")};

// What finishes a staged update after the event loop. main.cpp takes it
// while the QML engine lives and runs it once the engine is gone, so the
// settings files are written before they are carried over
struct ExitPlan {
    UpdateConfig::Method method{UpdateConfig::NoMethod}; // NoMethod: nothing staged
    QString version;
    QString installRoot;
    QString stagedRoot;
    QString exeName;
    QString appImagePath;
    QString updatesDir;
    QString helperScript;            // Windows: the text, written at exit
    QSet<QString> shippedFiles;
    // Held until the commit is done. Windows: only until main() returns;
    // then the helper's pid file keeps cleanup and staging off
    std::shared_ptr<QLockFile> lock;
};
// Carries user files over, then Linux: commits (fast renames); Windows:
// starts the helper, which relaunches YAYC on a \a restart. True: the
// helper runs, so main.cpp must not relaunch. A failure is written for the
// next session. Another running YAYC of this install: nothing happens, the
// stage waits. Needs no QCoreApplication
bool runExitPlan(const ExitPlan &plan, bool restart);

// Identifies a running process across PID reuse (creation time). Empty:
// not running, or not supported here
QString processStartToken(qint64 pid);

// running/<pid>.lock in \a updatesDir, held while it lives. nullptr: no lock
std::unique_ptr<QLockFile> registerRunning(const QString &updatesDir);
// Another live process holds a lock in running/. Removes those of dead ones
bool othersRunning(const QString &updatesDir);

// Commit steps of a staged update; return the error, empty on success.
// A .old in the way without the marker stops the commit (backupMarkerName).
// AppImage: UpdateLinux::commitAppImage(). Tarball: newRoot takes
// installRoot's place, the old tree stays as .old; a failed rollback keeps
// the new tree as <installRoot>.new (or .new2, ...: never over another)
QString commitDirSwap(const QString &installRoot, const QString &newRoot,
                      const QString &exeName);
// Clears the way for a new backup at \a old, then marks \a source, the
// tree (\a dir) or file about to become it. Returns the error, empty on
// success
QString prepareBackup(const QString &old, const QString &source, bool dir);
// Deletes \a old only if it has the marker. True: no .old is left
bool removeBackup(const QString &old);
// The marker of a backup at \a old: inside a dir, next to a file
QString backupMarker(const QString &old, bool dir);
// Where a directory swap extracts the new tree: <installRoot>.staging
QString stagingDir(const QString &installRoot);
// A backup may go only while the live install it protects is intact
bool isSafeToRemoveBackup(const QString &liveExecutable);

// Copies every file of oldRoot that is not shipped and not in newRoot
// (user data) into newRoot. Returns the error, empty on success
QString carryOverUserFiles(const QString &oldRoot, const QString &newRoot,
                           const QSet<QString> &shipped);
// yayc-manifest.txt: one path per line, relative to the install root
QSet<QString> readManifest(const QString &path);
// A manifest path as compared: / separators; lower case on Windows
QString manifestKey(QString relPath);
// fwrite + fflush, byte for byte (Windows: no \n → \r\n)
bool writeBytes(std::FILE *out, const QByteArray &data);
// Writes straight to \a dst (no temp name), so leftovers keep known names.
// Stops early (false) once *abort turns true
bool copyFile(const QString &src, const QString &dst, const std::atomic_bool *abort = nullptr);

} // namespace UpdateInstall

#endif // UPDATEINSTALL_H
