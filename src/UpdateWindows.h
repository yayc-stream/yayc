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

#ifndef UPDATEWINDOWS_H
#define UPDATEWINDOWS_H

#include "UpdateInstall.h"

#include <QString>
#include <QStringList>

// The Windows helper: a cmd script that installs a staged update once YAYC
// is gone. Builds on every OS, so the unit tests check the script text
namespace UpdateWindows {

// Waits until no YAYC of this install runs (UpdateInstall::registerRunning),
// up to a minute, then swaps \a newRoot into \a installRoot, if it still
// holds the exe, or runs \a installer, if it still has \a installerSha256.
// Starts \a exePath with \a relaunchArgs when called with argument 1, or
// when a start asked (relaunch-request). Exactly one of newRoot / installer
// is set. UTF-8. A failure writes last-install-error.json for \a version
QString buildHelperScript(const QString &version, const QString &exePath,
                          const QString &installRoot, const QString &newRoot,
                          const QString &installer, const QString &installerSha256,
                          const QStringList &relaunchArgs);
// Writes plan.helperScript into updates/, starts it detached and records its
// pid. A swap also gets a RunOnce value: a helper killed mid-swap (logoff,
// antivirus) runs again at the next logon. Returns the error, empty on
// success; elsewhere than Windows, always one
QString startHelper(const UpdateInstall::ExitPlan &plan, bool restart);
// The RunOnce value of the swap helper of \a installRoot: one per install
QString runOnceName(const QString &installRoot);
// Removes it (an orphaned helper). Windows only, else nothing
void clearRunOnce(const QString &installRoot);

} // namespace UpdateWindows

#endif // UPDATEWINDOWS_H
