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

#ifndef UPDATELINUX_H
#define UPDATELINUX_H

#include <QString>
#include <QStringList>

// The Linux install methods: deb (pkexec or a command for the user), AppImage
namespace UpdateLinux {

// pkexec arguments: root copies \a package out of the user's reach, then
// installs the copy only if it has \a sha256. The cache is user-writable,
// and the password prompt can wait for minutes
QStringList debInstallCommand(const QString &package, const QString &sha256);
// The same install for a user without pkexec, as one shell command: sudo,
// then pkill \a exeName
QString manualDebCommand(const QString &package, const QString &sha256, const QString &exeName);
// A complete single-quoted POSIX sh literal
QString shellQuote(const QString &s);

// <target>.new replaces <target>, the old one stays as .old (marked, see
// UpdateInstall::prepareBackup()). Returns the error, empty on success
QString commitAppImage(const QString &target);

} // namespace UpdateLinux

#endif // UPDATELINUX_H
