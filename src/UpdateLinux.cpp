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

#include "UpdateLinux.h"

#include "UpdateInstall.h"

#include <QFile>
#include <QFileInfo>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <cstring>
#include <stdio.h>
#include <unistd.h>
#endif

namespace UpdateLinux {

using UpdateInstall::copyFile;
using UpdateInstall::newSuffix;
using UpdateInstall::oldSuffix;
using UpdateInstall::backupMarker;
using UpdateInstall::prepareBackup;

QStringList debInstallCommand(const QString &package, const QString &sha256)
{
    // $1: package, $2: SHA-256. Root checks a copy in a root-only dir of the
    // sticky /tmp. System PATH: sudo may keep the user's. dd opens a link,
    // FIFO or dir swapped in after the test with an error, never blocking;
    // the cap (1 GiB) guards /tmp, often RAM. The dir goes on Ctrl-C too.
    // Double quotes only: the manual command puts it in single quotes.
    // One line: the user pastes the manual command
    static const QString script = QStringLiteral(R"sh(PATH=/usr/sbin:/usr/bin:/sbin:/bin; set -e;
[ -f "$1" ] && [ ! -h "$1" ] || { echo "the package is not a regular file" >&2; exit 1; };
d=$(mktemp -d /tmp/yayc-update.XXXXXXXX); trap "rm -rf \"$d\"" EXIT; trap "exit 1" INT TERM HUP;
(ulimit -f 2097152; dd if="$1" of="$d/yayc.deb" bs=1M iflag=nofollow,nonblock status=none);
echo "$2  $d/yayc.deb" | sha256sum -c --strict --status || { echo "the package changed after its checksum check" >&2; exit 1; };
dpkg -i "$d/yayc.deb")sh").replace(QLatin1Char('\n'), QLatin1Char(' '));
    return {QStringLiteral("/bin/sh"), QStringLiteral("-c"), script, QStringLiteral("sh"),
            package, sha256};
}

QString manualDebCommand(const QString &package, const QString &sha256, const QString &exeName)
{
    // By path: a fake sudo in the user's PATH would get the password
    QStringList words{QStringLiteral("/usr/bin/sudo")};
    for (const QString &arg : debInstallCommand(package, sha256))
        words << shellQuote(arg);
    // pkill: the app still runs the files dpkg replaced. Only after a
    // successful install, so a failed one leaves the app running
    return words.join(QLatin1Char(' ')) + QLatin1String(" && pkill -x ") + shellQuote(exeName);
}

QString shellQuote(const QString &s)
{
    QString escaped = s;
    escaped.replace(QLatin1Char('\''), QLatin1String("'\\''"));
    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

QString commitAppImage(const QString &target)
{
#ifdef Q_OS_UNIX
    const QString staged = target + newSuffix;
    const QString backup = target + oldSuffix;
    const QFileInfo stagedInfo(staged);
    if (target.isEmpty() || !stagedInfo.isFile() || !stagedInfo.isExecutable())
        return QLatin1String("staged AppImage missing or not executable: ") + staged;
    if (const QString error = prepareBackup(backup, target, false); !error.isEmpty())
        return error;
    const QString marker = backupMarker(backup, false);

    // Hard link: instant backup. Copy on filesystems without links
    const QByteArray targetPath = QFile::encodeName(target);
    if (::link(targetPath.constData(), QFile::encodeName(backup).constData()) != 0
            && (!copyFile(target, backup)
                || !QFile::setPermissions(backup, QFile::permissions(target)))) {
        QFile::remove(backup);
        QFile::remove(marker);
        return QLatin1String("cannot back up current AppImage");
    }
    // Atomic: the target is always one complete image
    if (::rename(QFile::encodeName(staged).constData(), targetPath.constData()) != 0) {
        const QString err = QString::fromLocal8Bit(std::strerror(errno));
        QFile::remove(backup);
        QFile::remove(marker);
        return QLatin1String("cannot replace AppImage: ") + err;
    }
    return {};
#else
    Q_UNUSED(target)
    return QLatin1String("AppImage update is Linux only");
#endif
}

} // namespace UpdateLinux
