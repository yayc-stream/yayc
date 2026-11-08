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

#include "UpdateWindows.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>

#ifdef Q_OS_WIN
#include <QProcess>
#include <QSettings>
#include <qt_windows.h>
#endif

namespace UpdateWindows {

using UpdateInstall::ExitPlan;
using UpdateInstall::helperPidName;
using UpdateInstall::helperScriptName;
using UpdateInstall::installErrorName;
using UpdateInstall::journalName;
using UpdateInstall::oldSuffix;
using UpdateInstall::processStartToken;

namespace {

// In updates/: the installer hash, as certutil prints it
const QString hashFileName{QStringLiteral("yayc-update.sha256")};
// For reg.exe in the script, and for QSettings
const QString runOnceKey{QStringLiteral("HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce")};
const QString runOnceSettings{
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce")};
// Next to the install, where the user looks when YAYC is gone
const QString failNoteSuffix{QStringLiteral("-UPDATE-FAILED.txt")};

// The helper scripts as they run. Rules for both:
// - No parenthesised blocks: cmd expands %vars% once per block, which
//   breaks counters inside them
// - In a .cmd, set resets errorlevel: test it first. equ 0, not
//   "not errorlevel 1": that also holds for negative codes (a crash)
// - Tools by path: the working dir (user-writable updates/) comes before PATH
// - for: its ( ) hold only the file set; the body is a call
// {name}: a value, see fill(). :: lines: notes, removed before use

constexpr char swapScript[] = R"cmd(@echo off
:: The file is UTF-8: non-ASCII user names in paths
"%SystemRoot%\System32\chcp.com" 65001 >nul
:: Out of YAYC's working directory: a folder that is any process's
:: current directory cannot be moved
cd /d "%~dp0"
:: Argument 2: RunOnce at logon, this run was killed (logoff, antivirus)
if "%~1"=="2" goto resume
:start
:: Each YAYC of this install, the one that started this too, holds
:: running\<pid>.lock: a QLockFile, which takes no other writer. Up to
:: 60 s: a YAYC started since keeps its files, and the stage waits
set waits=0
:wait
:: ping, not timeout: timeout fails at once without a console stdin
"%SystemRoot%\System32\ping.exe" -n 2 127.0.0.1 >nul
set busy=0
for %%f in ("{runningDir}\*.lock") do call :probe "%%~f"
if %busy% equ 0 goto ready
set /a waits+=1
if %waits% lss 60 goto wait
goto skip
:probe
if not exist "%~1" goto :eof
2>nul 9>>"%~1" type nul || set busy=1
goto :eof
:ready
:: No live tree (a resume with a lost journal): the disk decides, and
:: the .old, maybe the only copy, never goes
if not exist "{installRoot}\{exeName}" goto resumeaside
:: Hours may pass after the stage (antivirus, the user)
set "err=staged tree does not contain {exeName}"
if not exist "{newRoot}\{exeName}" goto fail
:: move into an existing dir nests inside it: clear the previous
:: backup, only if it has our marker, and skip the swap when that fails
if not exist "{backup}" goto mark
set "err={backupName} was not made by the updater: move it away, then retry"
if not exist "{backup}\{backupMarker}" goto fail
rmdir /s /q "{backup}"
set "err=cannot remove the previous backup"
if not exist "{backup}" goto mark
:: Partly deleted, maybe the marker first: back, or it passes for the user's
>"{backup}\{backupMarker}" echo {version}
goto fail
:: The marker goes along with the move. Unmarked, a .old would block
:: every later swap
:mark
>"{installRoot}\{backupMarker}" echo {version}
set "err=cannot mark the backup"
if not exist "{installRoot}\{backupMarker}" goto fail
:: The journal: the move under way, for a resume
>"{journal}" echo aside
:: DLLs can stay locked for a moment after the exit
set tries=0
:swap
move "{installRoot}" "{backup}" >nul 2>&1
if %errorlevel% equ 0 goto moved
set /a tries+=1
set "err=cannot move the current installation aside"
if %tries% geq 30 goto unrecord
"%SystemRoot%\System32\ping.exe" -n 2 127.0.0.1 >nul
goto swap
:: An antivirus may scan the new tree, or the old one, for a while
:moved
>"{journal}" echo in
set tries=0
:movein
move "{newRoot}" "{installRoot}" >nul 2>&1
if %errorlevel% equ 0 goto done
set /a tries+=1
set "err=cannot move the new installation into place"
if %tries% geq 30 goto rollback
"%SystemRoot%\System32\ping.exe" -n 2 127.0.0.1 >nul
goto movein
:rollback
set tries=0
:moveback
move "{backup}" "{installRoot}" >nul 2>&1
if %errorlevel% equ 0 goto unrecord
set /a tries+=1
if %tries% geq 30 goto broken
"%SystemRoot%\System32\ping.exe" -n 2 127.0.0.1 >nul
goto moveback
:: Where a killed run stopped: the journal, checked on disk
:resume
set "err=the install did not finish"
set "step="
2>nul set /p step=<"{journal}"
if "%step%"=="aside" goto resumeaside
if "%step%"=="in" goto resumein
goto start
:resumeaside
if exist "{installRoot}\{exeName}" goto start
if exist "{backup}\{exeName}" goto moved
goto broken
:resumein
if exist "{installRoot}\{exeName}" if not exist "{newRoot}\{exeName}" goto done
:: Rolled back
if exist "{installRoot}\{exeName}" goto unrecord
if exist "{newRoot}\{exeName}" goto moved
:: No tree in place. A note where the user looks; RunOnce and this file
:: stay, so the next logon tries again
:broken
set "err=YAYC is not in its folder: see {failNoteName} next to it"
>"{failNote}" echo YAYC could not finish an update. The old version is in "{backup}", the new one in "{newRoot}". Log on again to retry, or move one of them to "{installRoot}".
>"{installError}" echo {"version":"{version}","error":"%err%"}
exit /b 1
:: The backup goes in the next session, once the new version runs.
:: The error file: read by the next session, as after a failed commit on
:: Linux. YAYC wrote "did not finish" before this started
:done
del /q "{installError}" >nul 2>&1
del /q "{failNote}" >nul 2>&1
goto finish
:: No backup was left: the old tree, with the marker, is in place
:unrecord
del /q "{installRoot}\{backupMarker}" >nul 2>&1
:fail
>"{installError}" echo {"version":"{version}","error":"%err%"}
:finish
:: A start from now on runs: it no longer sees this helper
del /q "{helperPid}" >nul 2>&1
del /q "{journal}" >nul 2>&1
"%SystemRoot%\System32\reg.exe" delete "{runOnceKey}" /v "{runOnceName}" /f >nul 2>&1
:: Also a YAYC started while this ran: it quit for this, see deferToHelper()
if exist "{relaunchRequest}" goto relaunch
if not "%~1"=="1" goto end
:relaunch
del /q "{relaunchRequest}" >nul 2>&1
:: /D: the working dir of a normal restart, not updates/
start "" /D "{installRoot}" "{exe}"{relaunchArgs}
goto end
:: No swap: a YAYC runs (no relaunch then), and nothing failed. A later
:: exit installs
:skip
del /q "{relaunchRequest}" >nul 2>&1
del /q "{installError}" >nul 2>&1
"%SystemRoot%\System32\reg.exe" delete "{runOnceKey}" /v "{runOnceName}" /f >nul 2>&1
:end
del "%~f0"
)cmd";

constexpr char installerScript[] = R"cmd(@echo off
:: The file is UTF-8: non-ASCII user names in paths
"%SystemRoot%\System32\chcp.com" 65001 >nul
:: Out of YAYC's working directory: a folder that is any process's
:: current directory cannot be moved
cd /d "%~dp0"
:: Each YAYC of this install, the one that started this too, holds
:: running\<pid>.lock: a QLockFile, which takes no other writer. Up to
:: 60 s: a YAYC started since keeps its files, and the stage waits
set waits=0
:wait
:: ping, not timeout: timeout fails at once without a console stdin
"%SystemRoot%\System32\ping.exe" -n 2 127.0.0.1 >nul
set busy=0
for %%f in ("{runningDir}\*.lock") do call :probe "%%~f"
if %busy% equ 0 goto ready
set /a waits+=1
if %waits% lss 60 goto wait
goto skip
:probe
if not exist "%~1" goto :eof
2>nul 9>>"%~1" type nul || set busy=1
goto :eof
:ready
:: Admin runs this user-writable file, maybe days after its check: hash
:: it once more right before. Left open, by choice: a swap during the
:: UAC prompt (UAC is no security boundary; any process can raise one).
:: A file, not a pipe: each side of a pipe is parsed again, so the
:: %% in a path would expand twice
"%SystemRoot%\System32\certutil.exe" -hashfile "{installer}" SHA256 >"{hashFile}" 2>nul
"%SystemRoot%\System32\find.exe" /i "{installerSha256}" "{hashFile}" >nul
if %errorlevel% equ 0 goto verified
set "err=the installer changed after its checksum check"
goto fail
:verified
:: A declined UAC prompt fails too
"{installer}" /SILENT /NORESTART
if %errorlevel% equ 0 goto done
set "err=the installer failed or was cancelled"
goto fail
:: The error file: read by the next session, as after a failed commit on
:: Linux. YAYC wrote "did not finish" before this started
:done
del /q "{installError}" >nul 2>&1
goto launch
:fail
>"{installError}" echo {"version":"{version}","error":"%err%"}
:launch
:: A start from now on runs: it no longer sees this helper
del /q "{helperPid}" >nul 2>&1
:: Also a YAYC started while this ran: it quit for this, see deferToHelper()
if exist "{relaunchRequest}" goto relaunch
if not "%~1"=="1" goto end
:relaunch
del /q "{relaunchRequest}" >nul 2>&1
:: /D: the working dir of a normal restart, not updates/
start "" /D "{installRoot}" "{exe}"{relaunchArgs}
goto end
:: Not run: a YAYC runs (no relaunch then), and nothing failed. The
:: package stays on offer
:skip
del /q "{relaunchRequest}" >nul 2>&1
del /q "{installError}" >nul 2>&1
:end
del /q "{hashFile}" >nul 2>&1
del "%~f0"
)cmd";

// \ separators; %% keeps a % literal. Explicit: toNativeSeparators() is a
// no-op when not built for Windows
QString win(QString path)
{
    path.replace(QLatin1Char('/'), QLatin1Char('\\'));
    return path.replace(QLatin1Char('%'), QLatin1String("%%"));
}

// Two parsers read each relaunch argument. First YAYC's (CommandLineToArgvW):
// backslashes are doubled only before a quote, the closing one too, and
// \" is a literal quote
QString argvQuote(const QString &arg)
{
    QString quoted(QLatin1Char('"'));
    qsizetype slashes = 0;
    for (const QChar c : arg) {
        if (c == QLatin1Char('"'))
            quoted += QString(slashes + 1, QLatin1Char('\\'));
        slashes = c == QLatin1Char('\\') ? slashes + 1 : 0;
        quoted += c;
    }
    return quoted + QString(slashes, QLatin1Char('\\')) + QLatin1Char('"');
}

// Then cmd's: %% is a literal percent anywhere; ^ keeps & | < > ( ) ^
// literal where a \" left cmd outside quotes
QString cmdEscape(const QString &args)
{
    QString escaped;
    bool inQuotes = false;
    for (const QChar c : args) {
        if (c == QLatin1Char('"'))
            inQuotes = !inQuotes;
        else if (c == QLatin1Char('%'))
            escaped += c;
        else if (!inQuotes && QStringView(u"&|<>()^").contains(c))
            escaped += QLatin1Char('^');
        escaped += c;
    }
    return escaped;
}

// Each {name} in \a text becomes values[name]. One pass: a value is never
// read again, so a path holding "{exe}" stays as it is. Every {name} needs
// a value and every value a {name} (asserted)
QString fill(const QString &text, const QHash<QString, QString> &values)
{
    static const QRegularExpression placeholder(QStringLiteral("\\{([A-Za-z_]\\w*)\\}"));
    QString out;
    QSet<QString> used;
    qsizetype from = 0;
    for (const QRegularExpressionMatch &m : placeholder.globalMatch(text)) {
        const auto value = values.constFind(m.captured(1));
        Q_ASSERT_X(value != values.cend(), "fill", qPrintable(m.captured(0)));
        if (value == values.cend())
            continue;
        out += QStringView(text).sliced(from, m.capturedStart() - from);
        out += *value;
        from = m.capturedEnd();
        used.insert(value.key());
    }
    Q_ASSERT_X(used.size() == values.size(), "fill", "a value without its {name}");
    return out + QStringView(text).sliced(from);
}

QString script(const char *text, const QHash<QString, QString> &values)
{
    static const QRegularExpression note(QStringLiteral("^::[^\\n]*\\n"),
                                         QRegularExpression::MultilineOption);
    QString s = QString::fromUtf8(text);
    s.remove(QLatin1Char('\r')); // a CRLF checkout of this file: no \r\r\n
    s.remove(note);
    // Before fill(): values keep their bytes
    s.replace(QLatin1Char('\n'), QLatin1String("\r\n"));
    return fill(s, values);
}

} // namespace

QString buildHelperScript(const QString &version,
                          const QString &exePath,
                          const QString &installRoot,
                          const QString &newRoot,
                          const QString &installer,
                          const QString &installerSha256,
                          const QStringList &relaunchArgs)
{
    QString args;
    for (const QString &a : relaunchArgs)
        args += QLatin1Char(' ') + argvQuote(a);
    const QString inst = win(installRoot);
    QHash<QString, QString> values{
            {QStringLiteral("version"), version},
            {QStringLiteral("runningDir"), UpdateInstall::runningDirName},
            {QStringLiteral("relaunchRequest"), UpdateInstall::relaunchRequestName},
            {QStringLiteral("helperPid"), helperPidName},
            {QStringLiteral("exe"), win(exePath)},
            {QStringLiteral("installRoot"), inst},
            {QStringLiteral("relaunchArgs"), cmdEscape(args)},
            {QStringLiteral("installError"), installErrorName}};
    if (!newRoot.isEmpty()) {
        values.insert(QStringLiteral("newRoot"), win(newRoot));
        values.insert(QStringLiteral("exeName"), win(QFileInfo(exePath).fileName()));
        values.insert(QStringLiteral("backup"), inst + oldSuffix);
        values.insert(QStringLiteral("backupName"),
                      inst.mid(inst.lastIndexOf(QLatin1Char('\\')) + 1) + oldSuffix);
        values.insert(QStringLiteral("backupMarker"), UpdateInstall::backupMarkerName);
        values.insert(QStringLiteral("journal"), journalName);
        values.insert(QStringLiteral("runOnceKey"), runOnceKey);
        values.insert(QStringLiteral("runOnceName"), runOnceName(installRoot));
        values.insert(QStringLiteral("failNote"), inst + failNoteSuffix);
        values.insert(QStringLiteral("failNoteName"),
                      inst.mid(inst.lastIndexOf(QLatin1Char('\\')) + 1) + failNoteSuffix);
        return script(swapScript, values);
    }
    values.insert(QStringLiteral("installer"), win(installer));
    values.insert(QStringLiteral("installerSha256"), installerSha256);
    values.insert(QStringLiteral("hashFile"), hashFileName);
    return script(installerScript, values);
}

QString runOnceName(const QString &installRoot)
{
    // The key of its updates/ dir: same install, same name. "!": Windows
    // deletes it after the run, not before, so a killed resume runs again
    return QLatin1String("!YAYC update ")
            + QFileInfo(UpdateConfig::updatesDirFor(QString(), installRoot)).fileName();
}

void clearRunOnce([[maybe_unused]] const QString &installRoot)
{
#ifdef Q_OS_WIN
    QSettings(runOnceSettings, QSettings::NativeFormat).remove(runOnceName(installRoot));
#endif
}

QString startHelper(const ExitPlan &plan, bool restart)
{
    const QString script = plan.updatesDir + QLatin1Char('/') + helperScriptName;
    const QString pidFile = plan.updatesDir + QLatin1Char('/') + helperPidName;
    QFile::remove(pidFile); // a stale one would vouch for this new script
    {
        // The error of a buffered write only shows at flush; closed before
        // any remove: Windows cannot delete an open file
        QFile f(script);
        const QByteArray data = plan.helperScript.toUtf8();
        const bool written = f.open(QIODevice::WriteOnly | QIODevice::Truncate)
                && f.write(data) == data.size() && f.flush();
        f.close();
        if (!written) {
            QFile::remove(script);
            return QLatin1String("cannot write the update helper script");
        }
    }
#ifdef Q_OS_WIN
    // By path: a bare name is looked up in the app dir and the working dir first
    wchar_t systemDir[MAX_PATH];
    const UINT n = GetSystemDirectoryW(systemDir, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        QFile::remove(script);
        return QLatin1String("cannot find cmd.exe");
    }
    // cmd /c ""script" flag": cmd strips only the outer quotes, so & or (
    // in the path stay literal. /e:on /v:off: the registry may turn command
    // extensions off or delayed expansion (eats ! in paths) on.
    // Flag 1: the helper relaunches YAYC. 2: a resume (RunOnce)
    QProcess helper;
    const QString cmdExe = QString::fromWCharArray(systemDir, n) + QLatin1String("\\cmd.exe");
    helper.setProgram(cmdExe);
    // No console window: the script sends all output to nul
    helper.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a) {
        a->flags = (a->flags & ~DWORD(CREATE_NEW_CONSOLE)) | CREATE_NO_WINDOW;
    });
    static const QString args = QStringLiteral(R"cmd(/d /e:on /v:off /c ""%1" %2")cmd");
    const QString nativeScript = QDir::toNativeSeparators(script);
    helper.setNativeArguments(args.arg(nativeScript,
                                       restart ? QStringLiteral("1") : QStringLiteral("0")));
    // Before the start: a helper may be killed at once
    const bool swap = plan.method == UpdateConfig::HelperSwap;
    if (swap) {
        QSettings runOnce(runOnceSettings, QSettings::NativeFormat);
        runOnce.setValue(runOnceName(plan.installRoot),
                         QLatin1Char('"') + cmdExe + QLatin1String("\" ")
                                 + args.arg(nativeScript, QStringLiteral("2")));
        runOnce.sync();
    }
    qint64 pid = 0;
    if (!helper.startDetached(&pid)) {
        clearRunOnce(plan.installRoot);
        QFile::remove(script);
        return QLatin1String("cannot start the update helper");
    }
    // Without it a live helper looks orphaned. Removing the script is safe:
    // the helper waits for this process, and stops at its next line.
    // A resumed one (RunOnce) has none: a YAYC start deletes it as an
    // orphan. It stops at its next line; no move got through, as the
    // running YAYC keeps its folder in use
    const QString token = processStartToken(pid);
    QFile f(pidFile);
    const QByteArray data = QByteArray::number(pid) + ' ' + token.toLatin1();
    const bool written = !token.isEmpty()
            && f.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && f.write(data) == data.size() && f.flush();
    f.close();
    if (!written) {
        clearRunOnce(plan.installRoot);
        QFile::remove(pidFile);
        QFile::remove(script);
        return QLatin1String("cannot record the update helper");
    }
    return {};
#else
    Q_UNUSED(restart)
    QFile::remove(script);
    return QLatin1String("the update helper runs on Windows only");
#endif
}

} // namespace UpdateWindows
