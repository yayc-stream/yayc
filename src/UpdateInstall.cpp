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

#include "UpdateInstall.h"

#include "UpdateLinux.h"
#include "UpdateWindows.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>

#include <utility>

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#include <qt_windows.h>
#endif

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace UpdateInstall {

namespace {

bool pathExists(const QString &path)
{
    const QFileInfo fi(path);
    return fi.exists() || fi.isSymbolicLink(); // a dangling link too
}

} // namespace

QString backupMarker(const QString &old, bool dir)
{
    return dir ? old + QLatin1Char('/') + backupMarkerName : old + backupMarkerName;
}

QString prepareBackup(const QString &old, const QString &source, bool dir)
{
    if (!removeBackup(old)) {
        const QFileInfo fi(old);
        if (QFileInfo::exists(backupMarker(old, fi.isDir() && !fi.isSymbolicLink())))
            return QLatin1String("cannot remove the previous backup ") + old;
        return old + QLatin1String(" was not made by the updater: move it away, then retry");
    }
    // Before the backup exists: a .old never goes unmarked. A dir's marker
    // goes along with the rename
    QFile f(dir ? backupMarker(source, true) : backupMarker(old, false));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QLatin1String("cannot mark the backup: ") + f.fileName();
    return {};
}

bool isSafeToRemoveBackup(const QString &liveExecutable)
{
    const QFileInfo fi(liveExecutable);
    return !liveExecutable.isEmpty() && fi.isFile() && fi.isExecutable();
}

bool removeBackup(const QString &old)
{
    const QString beside = backupMarker(old, false);
    if (!pathExists(old)) {
        QFile::remove(beside); // the backup is gone already
        return true;
    }
    const QFileInfo fi(old);
    const bool dir = fi.isDir() && !fi.isSymbolicLink();
    if (!QFileInfo::exists(backupMarker(old, dir)))
        return false;
    QFile::remove(old); // a link goes as a link
    QDir(old).removeRecursively();
    if (pathExists(old)) {
        // Partly deleted, maybe the marker first: put it back, or the
        // updater's .old would pass for the user's and block every commit
        if (dir) {
            QFile marker(backupMarker(old, true));
            if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate))
                marker.write("1");
        }
        return false;
    }
    QFile::remove(beside);
    return true;
}

bool copyFile(const QString &src, const QString &dst,
              const std::atomic_bool *abort)
{
    QFile in(src);
    QFile out(dst);
    if (!in.open(QIODevice::ReadOnly) || !out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    QByteArray buf(1 << 20, Qt::Uninitialized);
    for (;;) {
        if (abort && abort->load())
            return false;
        const qint64 n = in.read(buf.data(), buf.size());
        if (n < 0)
            return false;
        if (n == 0)
            break;
        if (out.write(buf.constData(), n) != n)
            return false;
    }
    if (!out.flush())
        return false;
#ifdef Q_OS_UNIX
    // On disk before a rename() can publish it: no empty file after power loss
    if (::fsync(out.handle()) != 0)
        return false;
#endif
    return true;
}
namespace {

void writeInstallError(const QString &path, const QString &version, const QString &error)
{
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonObject o;
        o.insert(QLatin1String("version"), version);
        o.insert(QLatin1String("error"), error);
        f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }
}

} // namespace

bool runExitPlan(const ExitPlan &plan, bool restart)
{
    if (plan.method == UpdateConfig::NoMethod)
        return false;
    // Others run from the files a swap moves, or the installer replaces. The
    // last one to quit installs (Windows portable: resumeStage())
    if ((UpdateConfig::isDirSwap(plan.method) || plan.method == UpdateConfig::Installer)
            && othersRunning(plan.updatesDir))
        return false;
    const QString errorPath = plan.updatesDir + QLatin1Char('/') + installErrorName;
    QString error;
    // The swap moves the whole dir away: user files in it go into the new one
    // first. Their copies get old if the swap waits: never resumed then
    if (UpdateConfig::isDirSwap(plan.method)) {
        QFile::remove(plan.updatesDir + QLatin1Char('/') + stageMarkerName);
        error = carryOverUserFiles(plan.installRoot, plan.stagedRoot, plan.shippedFiles);
    }
    if (error.isEmpty()) {
        switch (plan.method) {
        case UpdateConfig::ReplaceAppImage:
            error = UpdateLinux::commitAppImage(plan.appImagePath);
            break;
        case UpdateConfig::SwapDirectory:
            error = commitDirSwap(plan.installRoot, plan.stagedRoot, plan.exeName);
            break;
        case UpdateConfig::HelperSwap:
            // As commitDirSwap(): the stage may have lost it since (antivirus)
            if (!QFileInfo(plan.stagedRoot + QLatin1Char('/') + plan.exeName).isExecutable()) {
                error = QLatin1String("staged tree does not contain ") + plan.exeName;
                break;
            }
            Q_FALLTHROUGH();
        case UpdateConfig::Installer:
            // A helper that dies (blocked cmd, antivirus) leaves this; else
            // the next start would stage, quit and lose it again (resumeStage)
            writeInstallError(errorPath, plan.version, QStringLiteral("the install did not finish"));
            error = UpdateWindows::startHelper(plan, restart);
            if (error.isEmpty())
                return true; // it records its own outcome
            break;
        case UpdateConfig::NoMethod:
        case UpdateConfig::ElevatedDpkg:
        case UpdateConfig::ManualDeb:
        case UpdateConfig::OpenDmg:
            break; // never staged
        }
    }
    // Nobody to show it to now (release builds log nothing): the next session
    // reads it, shows it and does not retry by itself
    if (error.isEmpty()) {
        QFile::remove(errorPath);
        return false;
    }
    writeInstallError(errorPath, plan.version, error);
    return false;
}

QString processStartToken(qint64 pid)
{
#if defined(Q_OS_WIN)
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD(pid));
    if (!h)
        return {};
    FILETIME created, exited, kernel, user;
    DWORD code = 0;
    const bool running = GetProcessTimes(h, &created, &exited, &kernel, &user)
            && GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    if (!running)
        return {};
    return QString::number((quint64(created.dwHighDateTime) << 32) | created.dwLowDateTime);
#elif defined(Q_OS_LINUX)
    // Field 22, starttime. comm (field 2) may hold spaces: count after its ')'
    QFile f(QStringLiteral("/proc/%1/stat").arg(pid));
    if (pid <= 0 || !f.open(QIODevice::ReadOnly))
        return {};
    const QByteArray stat = f.readAll();
    const qsizetype paren = stat.lastIndexOf(')');
    if (paren < 0)
        return {};
    const QList<QByteArray> fields = stat.mid(paren + 2).split(' ');
    return fields.size() > 19 ? QString::fromLatin1(fields.at(19)) : QString();
#else
    Q_UNUSED(pid)
    return {};
#endif
}

namespace {

QString runningDir(const QString &updatesDir)
{
    return updatesDir + QLatin1Char('/') + runningDirName;
}

QString ownRunningName()
{
    return QString::number(QCoreApplication::applicationPid()) + QLatin1String(".lock");
}

} // namespace

std::unique_ptr<QLockFile> registerRunning(const QString &updatesDir)
{
    if (!QDir().mkpath(runningDir(updatesDir)))
        return nullptr;
    auto lock = std::make_unique<QLockFile>(runningDir(updatesDir) + QLatin1Char('/')
                                            + ownRunningName());
    lock->setStaleLockTime(0);
    // A crashed YAYC with this pid left it: its pid looks alive (it is ours)
    if (!lock->tryLock(0) && !(lock->removeStaleLockFile() && lock->tryLock(0)))
        return nullptr;
    return lock;
}

bool othersRunning(const QString &updatesDir)
{
    const QDir dir(runningDir(updatesDir));
    const QString own = ownRunningName();
    const auto entries = dir.entryList({QStringLiteral("*.lock")}, QDir::Files);
    for (const QString &name : entries) {
        if (name == own)
            continue;
        QLockFile probe(dir.filePath(name));
        probe.setStaleLockTime(0);
        if (probe.tryLock(0))
            continue; // a dead one's: taken, and gone at unlock
        // Unreadable: not a QLockFile's (the helper's probe may create one).
        // A fresh one may be a QLockFile not written yet (Unix)
        qint64 pid = 0;
        QString host;
        QString app;
        const QFileInfo fi(dir.filePath(name));
        if (probe.error() == QLockFile::LockFailedError && !probe.getLockInfo(&pid, &host, &app)
                && fi.lastModified().secsTo(QDateTime::currentDateTime()) > 60
                && QFile::remove(fi.filePath()))
            continue;
        return true;
    }
    return false;
}

bool writeBytes(std::FILE *out, const QByteArray &data)
{
#ifdef Q_OS_WIN
    // Text mode would turn each \n into \r\n: the bytes must stay as they are.
    // A GUI app has no stdout unless redirected (fd -2): _setmode would abort
    const int fd = _fileno(out);
    if (fd < 0 || _setmode(fd, _O_BINARY) == -1)
        return false;
#endif
    return std::fwrite(data.constData(), 1, size_t(data.size()), out) == size_t(data.size())
            && std::fflush(out) == 0;
}

QString manifestKey(QString relPath)
{
    relPath.replace(QLatin1Char('\\'), QLatin1Char('/'));
#ifdef Q_OS_WIN
    return relPath.toLower(); // NTFS names ignore case
#else
    return relPath;
#endif
}

QSet<QString> readManifest(const QString &path)
{
    QSet<QString> files;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return files;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (!line.isEmpty())
            files.insert(manifestKey(line));
    }
    return files;
}

QString carryOverUserFiles(const QString &oldRoot, const QString &newRoot,
                           const QSet<QString> &shipped)
{
    // Without the list, an old library would pass for user data
    if (shipped.isEmpty())
        return QLatin1String("no list of shipped files (yayc-manifest.txt)");
    const QDir old(oldRoot);
    // Dirs holding shipped files belong to the package
    QSet<QString> shippedDirs;
    for (const QString &file : shipped) {
        for (qsizetype i = file.lastIndexOf(QLatin1Char('/')); i > 0;
             i = file.lastIndexOf(QLatin1Char('/'), i - 1))
            shippedDirs.insert(file.left(i));
    }
    // Their own permissions only once filled: a read-only dir takes no files
    QList<std::pair<QString, QFileDevice::Permissions>> createdDirs;
    // Links are not followed: a linked data dir goes over as the link.
    // A dir comes before its content
    QDirIterator it(oldRoot, QDir::Files | QDir::Dirs | QDir::Hidden | QDir::System
                    | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo fi = it.nextFileInfo();
        const bool link = fi.isSymbolicLink() || fi.isJunction();
        const QString rel = old.relativeFilePath(fi.filePath());
        if (fi.isDir() && !link) {
            // Not listed, so not read: its files would go with .old
            bool listable = fi.isReadable();
#ifdef Q_OS_UNIX
            listable = listable && fi.isExecutable(); // x = may enter
#endif
            if (!listable)
                return QLatin1String("cannot read ") + rel;
            const QString dst = newRoot + QLatin1Char('/') + rel;
            // Also empty ones: a bookmark dir may be empty yet configured
            if (!shippedDirs.contains(manifestKey(rel)) && !QFileInfo::exists(dst)) {
                if (!QDir().mkpath(dst))
                    return QLatin1String("cannot carry over ") + rel;
                createdDirs.append({dst, fi.permissions()});
            }
            continue;
        }
        if (shipped.contains(manifestKey(rel)) || rel == backupMarkerName)
            continue;
        const QString dst = newRoot + QLatin1Char('/') + rel;
        const QFileInfo dstInfo(dst);
        if (dstInfo.exists() || dstInfo.isSymbolicLink())
            continue; // the package wins
        if (!QDir().mkpath(dstInfo.absolutePath()))
            return QLatin1String("cannot carry over ") + rel;
        bool ok = false;
        if (link) {
#ifdef Q_OS_UNIX
            // The raw target: a relative link stays relative
            ok = ::symlink(QFile::encodeName(fi.readSymLink()).constData(),
                           QFile::encodeName(dst).constData()) == 0;
#endif
            // Windows links and junctions: not carried, so no swap
        } else {
            ok = copyFile(fi.filePath(), dst) && QFile::setPermissions(dst, fi.permissions());
        }
        if (!ok)
            return QLatin1String("cannot carry over ") + rel;
    }
    // Deepest first
    for (auto it = createdDirs.crbegin(); it != createdDirs.crend(); ++it) {
        if (!QFile::setPermissions(it->first, it->second))
            return QLatin1String("cannot carry over ") + it->first;
    }
    return {};
}

QString commitDirSwap(const QString &installRoot, const QString &newRoot,
                      const QString &exeName)
{
    const QString staging = stagingDir(installRoot);
    // Layout drift protection: never swap in a tree without our binary
    if (newRoot.isEmpty() || !QFileInfo(newRoot + QLatin1Char('/') + exeName).isExecutable())
        return QLatin1String("staged tree does not contain ") + exeName;

    const QString old = installRoot + oldSuffix;
    if (const QString error = prepareBackup(old, installRoot, true); !error.isEmpty())
        return error;
    const QString marker = backupMarker(installRoot, true);
    if (!QDir().rename(installRoot, old)) {
        QFile::remove(marker);
        return QLatin1String("cannot move current installation aside");
    }
    if (!QDir().rename(newRoot, installRoot)) {
        if (!QDir().rename(old, installRoot)) {
            // Neither tree is in place. Keep both under names cleanup never
            // touches, and never over one (an earlier failure, the user's)
            QString kept = installRoot + newSuffix;
            for (int i = 2; pathExists(kept); ++i)
                kept = installRoot + newSuffix + QString::number(i);
            const bool keptNew = QDir().rename(newRoot, kept);
            QDir(staging).removeRecursively();
            return QLatin1String("cannot move new installation into place, and the rollback "
                                 "failed: YAYC is now at ") + old
                    + (keptNew ? QLatin1String(", the new version at ") + kept : QString());
        }
        QFile::remove(marker); // the old tree is back: no backup
        return QLatin1String("cannot move new installation into place; original restored");
    }
    QDir(staging).removeRecursively(); // a no-op when newRoot was the staging dir
    return {};
}

QString stagingDir(const QString &installRoot)
{
    // Next to the install: same filesystem (renames), and a name of its own
    return installRoot + QLatin1String(".staging");
}

} // namespace UpdateInstall
