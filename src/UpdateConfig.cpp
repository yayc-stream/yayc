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

#include "UpdateConfig.h"
#include "Platform.h"
#include "UpdateInstall.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryFile>

namespace {

// One arch, three spellings: uname -m (AppImage, tarball), dpkg, Windows.
// All empty: no release artifact for this arch
struct ArchNames {
    QString uname;
    QString deb;
    QString win;
};

ArchNames archNames(const QString &arch)
{
    if (arch == QLatin1String("x86_64"))
        return {QStringLiteral("x86_64"), QStringLiteral("amd64"), QStringLiteral("x64")};
    if (arch == QLatin1String("arm64"))
        return {QStringLiteral("aarch64"), QStringLiteral("arm64"), QStringLiteral("arm64")};
    return {};
}

} // namespace

bool UpdateConfig::isDirSwap(Method m)
{
    return m == SwapDirectory || m == HelperSwap;
}

bool UpdateConfig::isElevated(Method m)
{
    return m == ElevatedDpkg || m == ManualDeb || m == Installer;
}

UpdateConfig UpdateConfig::detect()
{
    UpdateConfig c;
    c.releasesUrl = repositoryURL + QLatin1String("/releases/download");
    c.latestVersionUrl = latestReleaseVersionURL;
    c.updateFeedUrl = updateFeedURL;
#ifdef YAYC_TEST_BUILD
    // Update test bench: releases from a local server
    const QString testServer = qEnvironmentVariable("YAYC_TEST_UPDATE_SERVER");
    if (!testServer.isEmpty()) {
        c.releasesUrl = testServer + QLatin1String("/releases/download");
        c.latestVersionUrl = testServer + QLatin1String("/APPVERSION");
        c.updateFeedUrl = testServer + QLatin1String("/update/latest.json");
    }
#endif
    c.currentVersion = QString::fromUtf8(appVersion());
    c.arch = QSysInfo::buildCpuArchitecture();
    c.exePath = QCoreApplication::applicationFilePath();
    c.installRoot = QCoreApplication::applicationDirPath();
    c.appImagePath = runningAppImage();
    c.installType = detectInstallType(c);
    c.shippedFiles = UpdateInstall::readManifest(UpdateInstall::manifestResource);
    c.updatesDir = updatesDirFor(QStandardPaths::writableLocation(QStandardPaths::CacheLocation),
                                 c.appImagePath.isEmpty() ? c.installRoot : c.appImagePath);
    computeUpdateMethod(c);
    return c;
}

UpdateConfig::InstallType UpdateConfig::detectInstallType([[maybe_unused]] const UpdateConfig &c)
{
#ifndef YAYC_DIST_BUILD
    // pass DEFINES+=YAYC_DIST_BUILD in packaging builds only
    return SelfBuilt;
#elif defined(Q_OS_LINUX)
    if (!c.appImagePath.isEmpty())
        return LinuxAppImage;
    // Package name = executable name. Ask dpkg, not the path: the deb is in /opt
    const QString dpkgInfo = QLatin1String("/var/lib/dpkg/info/") + QFileInfo(c.exePath).fileName();
    if (isListedByDpkg(dpkgInfo + QLatin1String(".list"), c.exePath)
            || isListedByDpkg(dpkgInfo + QLatin1Char(':') + archNames(c.arch).deb
                              + QLatin1String(".list"), c.exePath))
        return LinuxDeb;
    return LinuxTarball;
#elif defined(Q_OS_WIN)
    if (QFileInfo::exists(c.installRoot + QLatin1String("/unins000.exe")))
        return WindowsInstalled;
    return WindowsPortable;
#elif defined(Q_OS_MACOS)
    return MacOSBundle;
#else
    return Unknown;
#endif
}

QString UpdateConfig::runningAppImage()
{
    return runningAppImage(qEnvironmentVariable("APPIMAGE"), qEnvironmentVariable("APPDIR"),
                           QCoreApplication::applicationFilePath());
}

QString UpdateConfig::runningAppImage(const QString &appImage, const QString &appDir,
                                      const QString &exePath)
{
    if (appImage.isEmpty() || appDir.isEmpty())
        return {};
    const QString mount = QDir::cleanPath(appDir) + QLatin1Char('/');
    return exePath.startsWith(mount) ? appImage : QString();
}

bool UpdateConfig::isListedByDpkg(const QString &listFile, const QString &filePath)
{
    QFile f(listFile);
    if (filePath.isEmpty() || !f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray wanted = QFile::encodeName(filePath);
    while (!f.atEnd()) {
        QByteArray line = f.readLine();
        if (line.endsWith('\n'))
            line.chop(1);
        if (line == wanted)
            return true;
    }
    return false;
}

bool UpdateConfig::isYaycDir(const QString &dir)
{
    return QFileInfo(QDir::cleanPath(dir)).fileName().contains(QLatin1String("yayc"),
                                                               Qt::CaseInsensitive);
}

// QFileInfo::isWritable() misreads ACLs and read-only mounts; probe for real.
bool UpdateConfig::probeDirWritable(const QString &path)
{
    if (path.isEmpty() || !QFileInfo(path).isDir())
        return false;
    QTemporaryFile probe(path + QLatin1String("/.yayc-XXXXXX.probe"));
    return probe.open();
}

QString UpdateConfig::assetName(const QString &version) const
{
    if (installType == MacOSBundle)
        return "yayc-" + version + "-macos-universal.dmg"; // both archs in one
    const ArchNames a = archNames(arch);
    if (a.uname.isEmpty())
        return {};
    switch (installType) {
    case LinuxAppImage:    return "yayc-" + version + "-" + a.uname + ".AppImage";
    case LinuxTarball:     return "yayc-" + version + "-linux-" + a.uname + ".tar.xz";
    case LinuxDeb:         return "yayc_" + version + "_" + a.deb + ".deb";
    case WindowsPortable:  return "yayc-v" + version + "-win-" + a.win + "-portable.zip";
    case WindowsInstalled: return "yayc-v" + version + "-win-" + a.win + ".exe";
    default:               return {};
    }
}

void UpdateConfig::computeUpdateMethod(UpdateConfig &c)
{
    if (c.installType == SelfBuilt) {
        c.unsupportedReason = QLatin1String("self-built (packaged builds only)");
        return;
    }

    if (archNames(c.arch).uname.isEmpty()) {
        c.unsupportedReason = QLatin1String("no release artifact for CPU architecture ") + c.arch;
        return;
    }

    QDir().mkpath(c.updatesDir);
    if (!probeDirWritable(c.updatesDir)) {
        c.unsupportedReason = QLatin1String("cache directory not writable: ") + c.updatesDir;
        return;
    }

    const QString binDir = c.installRoot;
    switch (c.installType) {
    case LinuxAppImage: {
        const QString appImage = c.appImagePath;
        const QFileInfo fi(appImage);
        if (fi.isWritable() && probeDirWritable(fi.absolutePath()))
            c.updateMethod = ReplaceAppImage;
        else
            c.unsupportedReason = QLatin1String("AppImage location not writable: ") + appImage;
        break;
    }
    case LinuxTarball:
    case WindowsPortable: {
        // Directory swap needs rename rights in the parent, too
        const QString parent = QFileInfo(binDir).absolutePath();
        if (!isYaycDir(binDir))
            c.unsupportedReason = QLatin1String("install directory not named after YAYC: ") + binDir;
        else if (c.shippedFiles.isEmpty())
            c.unsupportedReason = QLatin1String("no list of shipped files (yayc-manifest.txt)");
        else if (probeDirWritable(binDir) && probeDirWritable(parent))
            c.updateMethod = c.installType == LinuxTarball ? SwapDirectory : HelperSwap;
        else
            c.unsupportedReason = QLatin1String("install directory not writable: ") + binDir;
        break;
    }
    case LinuxDeb: {
        // Only pkexec can ask for root from a GUI; else the user runs one command
        c.pkexecPath = QStandardPaths::findExecutable(QStringLiteral("pkexec"),
                                                      {QStringLiteral("/usr/bin"),
                                                       QStringLiteral("/bin")});
#ifdef YAYC_TEST_BUILD
        if (qEnvironmentVariableIsSet("YAYC_TEST_NO_PKEXEC"))
            c.pkexecPath.clear(); // update test bench: the manual-deb path
#endif
        c.updateMethod = c.pkexecPath.isEmpty() ? ManualDeb : ElevatedDpkg;
        break;
    }
    case WindowsInstalled:
        c.updateMethod = Installer; // elevates itself via UAC
        break;
    case MacOSBundle:
        c.updateMethod = OpenDmg;
        break;
    default:
        c.unsupportedReason = QLatin1String("unknown installation type");
        break;
    }
}

QString UpdateConfig::updatesDirFor(const QString &cacheRoot, const QString &install)
{
    // One folder per install: two installs of one user (installed + portable,
    // AppImage + deb) never touch each other's packages, lock or error file
    QString id = QDir::cleanPath(install);
#ifdef Q_OS_WIN
    id = id.toLower(); // a shortcut may spell the path in another case
#endif
    return cacheRoot + QLatin1String("/updates/")
            + QString::fromLatin1(QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Sha1)
                                  .toHex().left(12));
}
