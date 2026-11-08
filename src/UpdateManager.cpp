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

#include "UpdateManager.h"

#include "UpdateLinux.h"
#include "UpdateWindows.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QVersionNumber>

#include <utility>

namespace {

// In updates/: one instance per install does the update work. Two (-c a.ini,
// -c b.ini) would mix one .part, or clean up the other's stage
const QString ownerLockName{"owner.lock"};
// In updates/: what pkexec dpkg wrote
const QString dpkgLogName{"dpkg-error.log"};

// A file or a dir tree. Empty: nothing (QDir("") is the working dir)
void removePath(const QString &path)
{
    if (path.isEmpty())
        return;
    QFile::remove(path);
    QDir(path).removeRecursively();
}

// Linux commits are renames: a manual Apply restarts at once. The Windows
// helpers wait for an exit anyway
bool restartsAfterStage(UpdateConfig::Method m)
{
    return m == UpdateConfig::ReplaceAppImage || m == UpdateConfig::SwapDirectory;
}

bool isNewer(const QString &a, const QString &b)
{
    return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
}

} // namespace

UpdateManager::UpdateManager(QObject *parent)
    : UpdateManager(UpdateConfig::detect(), parent)
{
}

UpdateManager::UpdateManager(const UpdateConfig &config, QObject *parent)
    : QObject(parent),
      m_config(config)
{
    m_nam.setTransferTimeout(m_config.transferTimeoutMs);
    // Delayed: an update that crashes on startup must keep its .old fallback
    if (m_config.cleanupDelayMs >= 0)
        QTimer::singleShot(m_config.cleanupDelayMs, this, &UpdateManager::cleanupLeftovers);
}

UpdateManager::~UpdateManager()
{
    m_stageAbort->store(true);
    if (m_rootProcess) {
        // Still asking for the password, pkexec is the user's: it goes. As
        // root (dpkg runs) it ignores this; ~QProcess would wait 30 s for
        // it, and dpkg must finish anyway: leave it running
        m_rootProcess->terminate();
        QObject::disconnect(m_rootProcess, nullptr, this, nullptr);
        m_rootProcess->setParent(nullptr);
    }
    if (m_checkReply) {
        QObject::disconnect(m_checkReply, nullptr, this, nullptr);
        m_checkReply->abort();
    }
    m_fetcher.cancel();
}

UpdateConfig::InstallType UpdateManager::installType() const
{
    return m_config.installType;
}

bool UpdateManager::updateSupported() const
{
    return m_config.updateMethod != UpdateConfig::NoMethod;
}

QString UpdateManager::unsupportedReason() const
{
    return m_config.unsupportedReason;
}

UpdateConfig::Method UpdateManager::updateMethod() const
{
    return m_pkexecUnusable ? UpdateConfig::ManualDeb : m_config.updateMethod;
}

bool UpdateManager::checking() const
{
    return m_checkReply != nullptr;
}

bool UpdateManager::autoUpdate() const
{
    return m_autoUpdate;
}

void UpdateManager::setAutoUpdate(bool on)
{
    if (m_autoUpdate == on)
        return;
    m_autoUpdate = on;
    emit autoUpdateChanged();
    if (on) {
        startAutoInstall();
        return;
    }
    // Off: nothing installs behind the user's back. While Applying,
    // stageDone() undoes the stage when it ends
    if (m_applyIntent == ApplyIntent::Auto && (m_state == Verifying || m_state == ExitPending)) {
        dropAutoStage();
    } else if (m_autoStaged) {
        unstage(); // behind a newer download
        emit stateChanged(); // autoStaged
    }
    // A download the toggle started has no use now; a click's goes on
    if (m_state == Downloading && !m_downloadByUser)
        stopWork();
}

UpdateManager::UpdateState UpdateManager::state() const
{
    return m_state;
}

qreal UpdateManager::downloadProgress() const
{
    return m_downloadProgress;
}

void UpdateManager::setDownloadProgress(qreal progress)
{
    if (qFuzzyCompare(m_downloadProgress, progress))
        return;
    m_downloadProgress = progress;
    emit downloadProgressChanged();
}

QString UpdateManager::lastError() const
{
    return m_lastError;
}

QString UpdateManager::readyVersion() const
{
    return m_readyVersion;
}

QString UpdateManager::downloadVersion() const
{
    return m_downloadVersion;
}

QString UpdateManager::unavailableVersion() const
{
    return m_unavailableVersion;
}

bool UpdateManager::canRetry() const
{
    if (m_state == ReadyToInstall)
        return !m_lastError.isEmpty();
    // ExitPending: a newer download failed behind an automatic stage
    return (m_state == Failed || m_state == ExitPending) && !m_failedDownloadVersion.isEmpty();
}

bool UpdateManager::canCancel() const
{
    return m_state == Downloading || m_state == Verifying
            || (m_state == Applying && m_rootProcess);
}

bool UpdateManager::autoStaged() const
{
    return m_autoStaged;
}

bool UpdateManager::canInstallUnverified() const
{
    // Root or admin installs only bytes feed and checksums confirmed this
    // session: a file found in the cache may be a same-user process's
    return m_state == NotVerified
            && (!UpdateConfig::isElevated(updateMethod()) || !packageSha256().isEmpty());
}

bool UpdateManager::retryDownloads() const
{
    return canRetry() && !m_failedDownloadVersion.isEmpty();
}

QString UpdateManager::cacheDir(const QString &version) const
{
    return updatesFile(version);
}

QString UpdateManager::updateFilePath(const QString &version) const
{
    const QString asset = m_config.assetName(version);
    if (asset.isEmpty())
        return {};
    return cacheDir(version) + QLatin1Char('/') + asset;
}

bool UpdateManager::isUpdateDownloaded(const QString &version) const
{
    const QString path = updateFilePath(version);
    if (path.isEmpty())
        return false;
    const QFileInfo fi(path);
    return fi.isFile() && fi.size() > 0;
}

bool UpdateManager::isValidVersion(const QString &version)
{
    // anchoredPattern: a plain $ also matches before a trailing newline
    static const QRegularExpression re(
                QRegularExpression::anchoredPattern(QStringLiteral("[0-9]+\\.[0-9]+\\.[0-9]+")));
    return re.match(version).hasMatch();
}

QString UpdateManager::manualInstallCommand() const
{
    return m_state == ManualInstall ? m_manualCommand : QString();
}

bool UpdateManager::updatesElsewhere() const
{
    return m_updatesElsewhere;
}

bool UpdateManager::ensureOwner()
{
    if (m_ownerLock)
        return true;
    QDir().mkpath(updatesDir());
    auto lock = std::make_unique<QLockFile>(updatesFile(ownerLockName));
    // Held for the session: only a dead owner makes it stale, never its age
    lock->setStaleLockTime(0);
    const bool taken = lock->tryLock(0);
    // Only a holder means another window; an unwritable cache is no owner
    const bool elsewhere = !taken && lock->error() == QLockFile::LockFailedError;
    if (taken)
        m_ownerLock = std::move(lock);
    if (elsewhere != m_updatesElsewhere) {
        m_updatesElsewhere = elsewhere;
        emit stateChanged();
        // Taken over from an instance that quit: its package is on offer here
        if (!elsewhere)
            QTimer::singleShot(0, this, &UpdateManager::resumeDownloaded);
    }
    return taken;
}

UpdateInstall::ExitPlan UpdateManager::takeExitPlan()
{
    UpdateInstall::ExitPlan plan;
    if (m_state != ExitPending && !m_autoStaged)
        return plan;
    plan.method = m_config.updateMethod;
    plan.version = m_readyVersion;
    plan.installRoot = m_config.installRoot;
    if (UpdateConfig::isDirSwap(m_config.updateMethod))
        plan.stagedRoot = extractedRoot(stagedPath());
    plan.exeName = QFileInfo(m_config.exePath).fileName();
    plan.appImagePath = m_config.appImagePath;
    plan.updatesDir = updatesDir();
    plan.shippedFiles = m_config.shippedFiles;
    // Written at exit, so a session that ends without an exit leaves no
    // script behind
    const QStringList &args = m_config.relaunchArgs;
    if (m_config.updateMethod == UpdateConfig::HelperSwap)
        plan.helperScript = UpdateWindows::buildHelperScript(
                    m_readyVersion, m_config.exePath, m_config.installRoot, plan.stagedRoot,
                    QString(), QString(), args);
    else if (m_config.updateMethod == UpdateConfig::Installer) {
        // Never empty here (Apply needs it). An empty one would match any file
        const QString sha256 = packageSha256();
        if (sha256.isEmpty())
            return {};
        plan.helperScript = UpdateWindows::buildHelperScript(
                    m_readyVersion, m_config.exePath, m_config.installRoot, QString(),
                    updateFilePath(m_readyVersion), sha256, args);
    }
    plan.lock = std::shared_ptr<QLockFile>(std::move(m_ownerLock));
    return plan;
}

bool UpdateManager::finishOnExit(bool restart)
{
    return UpdateInstall::runExitPlan(takeExitPlan(), restart);
}

void UpdateManager::setState(UpdateState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void UpdateManager::setFailed(const QString &reason)
{
    m_lastError = reason;
    m_state = Failed;
    emit stateChanged();
}

QString UpdateManager::updatesDir() const
{
    return m_config.updatesDir;
}

QString UpdateManager::updatesFile(const QString &name) const
{
    return updatesDir() + QLatin1Char('/') + name;
}

/*
    Version check
*/

void UpdateManager::checkForUpdates()
{
    if (m_checkReply)
        return;
    if (updateSupported())
        ensureOwner(); // the owner may have quit
    m_checkReply = m_nam.get(UpdateFetcher::freshRequest(QUrl(m_config.latestVersionUrl)));
    emit checkingChanged();
    connect(m_checkReply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_checkReply;
        m_checkReply = nullptr;
        reply->deleteLater();
        emit checkingChanged();
        if (reply->error() != QNetworkReply::NoError) {
            emit checkFailed(reply->errorString());
            return;
        }
        const QString latest = QString::fromUtf8(reply->readAll()).trimmed();
        if (!isValidVersion(latest)) {
            emit checkFailed(QLatin1String("invalid version: ") + latest.left(32));
            return;
        }
        m_latestVersion = latest;
        emit latestVersionFound(latest);
        if (!isNewer(latest, m_config.currentVersion))
            emit upToDate();
        else if (m_autoUpdate && updateSupported() && m_unavailableVersion != latest
                 && m_declinedVersion != latest)
            requestDownload(latest, false); // a release without our package: asked once per session
    });
}

/*
    Pipeline
*/

void UpdateManager::startUpdate(const QString &version)
{
    requestDownload(version, true);
}

void UpdateManager::requestDownload(const QString &version, bool byUser)
{
    if (!updateSupported()) {
        setFailed(m_config.unsupportedReason);
        return;
    }
    if (!ensureOwner())
        return; // the UI says why
    const bool newer = !m_readyVersion.isEmpty() && isValidVersion(version)
            && isNewer(version, m_readyVersion);
    // One fetch at a time: a verify of the older package for an automatic
    // stage gives way
    if (newer && m_applyIntent == ApplyIntent::Auto && m_state == Verifying)
        dropAutoStage();
    // One outcome per session: staged/applied updates need a restart first.
    // A ready package only gives way to a newer release, and stays until
    // that one is ready. So does an automatic stage: a quit meanwhile
    // installs it
    const bool hasReady = m_state == ReadyToInstall || m_state == NotVerified
            || m_state == ManualInstall || m_autoStaged;
    const bool newerThanReady = hasReady && newer;
    if (m_state != Idle && m_state != Failed && !newerThanReady)
        return;
    if (m_fetcher.active())
        return;
    startDownload(version, byUser);
}

void UpdateManager::fetch(const QString &version, bool verifyOnly, UpdateFetcher::Done done)
{
    UpdateFetcher::Package pkg;
    QString error;
    if (!isValidVersion(version))
        error = QLatin1String("invalid version: ") + version;
    else if ((pkg.name = m_config.assetName(version)).isEmpty())
        error = QLatin1String("no release artifact for this installation type");
    else if (!QDir().mkpath(cacheDir(version)))
        error = QLatin1String("cannot create download directory");
    if (!error.isEmpty()) {
        done(Outcome::Local, error);
        return;
    }
    pkg.version = version;
    pkg.path = updateFilePath(version);
    pkg.updateFeedUrl = QUrl(m_config.updateFeedUrl);
    pkg.releasesUrl = m_config.releasesUrl;
    if (verifyOnly) {
        m_fetcher.verify(pkg, std::move(done));
        return;
    }
    setDownloadProgress(0);
    m_fetcher.download(pkg, std::move(done));
}

void UpdateManager::startDownload(const QString &version, bool byUser)
{
    m_downloadVersion = version;
    m_downloadByUser = byUser;
    // Only the ready package's own error stays: it must not install by itself
    m_lastError = m_readyError;
    setState(Downloading);
    fetch(version, false, [this](Outcome outcome, const QString &reason) {
        onDownloadDone(outcome, reason);
    });
}

void UpdateManager::onDownloadDone(Outcome outcome, const QString &reason)
{
    if (m_state != Downloading)
        return;
    const QString version = m_downloadVersion;
    m_downloadVersion.clear();
    const bool applyNow = m_applyAfterDownload;
    m_applyAfterDownload = false;
    if (outcome == Outcome::Ok) {
        if (m_unavailableVersion == version)
            m_unavailableVersion.clear();
        // Replaced by a newer one: nothing else uses the older package
        if (m_autoStaged)
            unstage();
        if (!m_readyVersion.isEmpty() && m_readyVersion != version)
            QDir(cacheDir(m_readyVersion)).removeRecursively();
        m_readyVersion = version;
        markVerified(version, m_fetcher.verifiedSha256()); // feed and checksums fetched just now
        m_failedDownloadVersion.clear();
        m_lastError.clear();
        m_readyError.clear();
        setState(ReadyToInstall);
        if (applyNow)
            applyUpdate(); // the user's Apply, interrupted by a re-download
        else
            startAutoInstall(); // toggle on: stage it for the quit
        return;
    }
    if (outcome == Outcome::NotListed || outcome == Outcome::Unpublished) {
        // No error. NotListed: not built for this install (e.g. a Windows-only
        // release), skipped this session. Unpublished: the next check retries
        if (outcome == Outcome::NotListed)
            m_unavailableVersion = version;
        backToReady(); // nothing to retry
        // A click on a release the feed has moved past: offer the feed's
        // instead of the same dead Download. The automatic check finds it
        const QString feedVersion = m_fetcher.feedVersion();
        if (outcome == Outcome::Unpublished && m_downloadByUser
                && isValidVersion(feedVersion) && isNewer(feedVersion, version)) {
            m_latestVersion = feedVersion;
            emit latestVersionFound(feedVersion);
        }
        return;
    }
    m_failedDownloadVersion = version;
    if (m_readyVersion.isEmpty()) {
        setFailed(reason);
        return;
    }
    // The older verified package is still good: offer it again (still staged,
    // if it was). Retry downloads
    m_lastError = reason;
    setState(m_autoStaged ? ExitPending : ReadyToInstall);
}

void UpdateManager::resumeDownloaded()
{
    if (m_state != Idle || !updateSupported() || !ensureOwner())
        return;
    // The newest one: the latest release may have none for this install
    QString best;
    const auto entries = QDir(updatesDir()).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &v : entries) {
        if (!isValidVersion(v) || !isUpdateDownloaded(v))
            continue;
        if (isNewer(v, m_config.currentVersion) && (best.isEmpty() || isNewer(v, best)))
            best = v;
    }
    if (best.isEmpty())
        return;
    m_readyVersion = best;
    m_readyError = installErrorFor(best);
    m_errorFromAuto = !m_readyError.isEmpty(); // an install at an exit
    m_lastError = m_readyError;
    setState(ReadyToInstall);
    startAutoInstall();
}

QString UpdateManager::installErrorFor(const QString &version) const
{
    QFile errorFile(updatesFile(UpdateInstall::installErrorName));
    if (!errorFile.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject o = QJsonDocument::fromJson(errorFile.readAll()).object();
    if (o.value(QLatin1String("version")).toString() != version)
        return {};
    // Never empty: it is what keeps the automatic install off
    const QString error = o.value(QLatin1String("error")).toString();
    return error.isEmpty() ? QStringLiteral("the install did not finish") : error;
}

bool UpdateManager::resumeStage()
{
    // Windows portable only. A session there often ends at logoff, where the
    // exit plan never runs (closing only hides to the tray). Linux: SIGTERM
    // runs it. The installer: no UAC prompt by itself (D4)
    if (m_config.updateMethod != UpdateConfig::HelperSwap || m_state != Idle)
        return false;
    QFile marker(updatesFile(UpdateInstall::stageMarkerName));
    const QString version = marker.open(QIODevice::ReadOnly)
            ? QString::fromUtf8(marker.readAll()).trimmed() : QString();
    const QString exe = extractedRoot(stagedPath()) + QLatin1Char('/')
            + QFileInfo(m_config.exePath).fileName();
    // After a failed install the error shows instead: no retry at every start
    if (!isValidVersion(version) || !isNewer(version, m_config.currentVersion)
            || !installErrorFor(version).isEmpty() || !QFileInfo(exe).isExecutable())
        return false;
    // Another YAYC of this install runs from these files: the last one installs
    if (!ensureOwner() || helperPending() || UpdateInstall::othersRunning(updatesDir()))
        return false;
    m_readyVersion = version;
    m_applyIntent = ApplyIntent::Auto;
    m_autoStaged = true;
    m_appliedThisSession = true;
    setState(ExitPending);
    return true;
}

bool UpdateManager::deferToHelper()
{
    if (m_config.updateMethod != UpdateConfig::HelperSwap
            && m_config.updateMethod != UpdateConfig::Installer)
        return false;
    if (!helperPending())
        return false;
    QFile f(updatesFile(UpdateInstall::relaunchRequestName));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false; // run instead: the helper then fails to swap, and says so
    return true;
}

bool UpdateManager::commitsAtExit() const
{
    return canStage() || m_config.updateMethod == UpdateConfig::Installer;
}

bool UpdateManager::canStage() const
{
    // Not the installer: it asks for UAC consent
    return m_config.updateMethod == UpdateConfig::ReplaceAppImage
            || UpdateConfig::isDirSwap(m_config.updateMethod);
}

void UpdateManager::startAutoInstall()
{
    // Password / UAC prompts and manual methods never start by themselves.
    // After a failure of this package only the user retries
    if (updateSupported() && !m_ownerLock)
        ensureOwner(); // networkFound: the owner may have quit
    if (!m_autoUpdate || !canStage() || m_readyVersion == m_declinedVersion
            || !m_readyError.isEmpty())
        return;
    // NotVerified: a click that found no network; the toggle goes on with it
    if (m_state == NotVerified && !m_fetcher.active()) {
        m_applyIntent = ApplyIntent::Auto;
        beginVerify();
        return;
    }
    apply(ApplyIntent::Auto);
}

void UpdateManager::applyUpdate()
{
    apply(ApplyIntent::Manual);
}

void UpdateManager::apply(ApplyIntent intent)
{
    if (m_state != ReadyToInstall || m_fetcher.active())
        return;
    m_applyIntent = intent;
    beginVerify();
}

void UpdateManager::markVerified(const QString &version, const QString &sha256)
{
    m_verifiedVersion = version;
    m_verifiedSha256 = sha256;
    m_verifiedAt.start();
}

QString UpdateManager::packageSha256() const
{
    return m_verifiedVersion == m_readyVersion ? m_verifiedSha256 : QString();
}

bool UpdateManager::isFreshlyVerified() const
{
    return !m_readyVersion.isEmpty() && m_verifiedVersion == m_readyVersion
            && m_verifiedAt.isValid() && m_verifiedAt.elapsed() < m_config.freshVerifyMs;
}

void UpdateManager::beginVerify()
{
    if (!isUpdateDownloaded(m_readyVersion)) {
        const QString pkg = updateFilePath(m_readyVersion);
        m_readyVersion.clear();
        m_readyError.clear();
        setFailed(QLatin1String("update package not found: ") + pkg);
        return;
    }
    // m_readyError stays until a definite outcome: a cancelled check keeps it
    m_lastError.clear();
    m_failedDownloadVersion.clear(); // Retry now means this package, not a download
    setState(Verifying);
    // Checked against feed and checksums fetched minutes ago: no second round trip
    if (isFreshlyVerified()) {
        onVerifyDone(Outcome::Ok, QString());
        return;
    }
    // The package may have sat in the cache since another session
    fetch(m_readyVersion, true, [this](Outcome outcome, const QString &reason) {
        // Only a real fetch restarts the window, never the shortcut
        if (outcome == Outcome::Ok)
            markVerified(m_readyVersion, m_fetcher.verifiedSha256());
        onVerifyDone(outcome, reason);
    });
}

void UpdateManager::applyUnverified()
{
    if (!canInstallUnverified())
        return;
    m_applyIntent = ApplyIntent::Manual;
    startApply();
}

void UpdateManager::retry()
{
    // Before canRetry(): it stays false here, or the banner would show
    // "Retry" next to "Retry check"
    if (m_state == NotVerified) {
        if (m_fetcher.active())
            return;
        m_applyIntent = ApplyIntent::Manual;
        beginVerify();
        return;
    }
    if (!canRetry())
        return;
    if (!m_failedDownloadVersion.isEmpty()) {
        // From Failed, or newer than ready. The release may have moved on since
        startUpdate(isNewer(m_latestVersion, m_failedDownloadVersion) ? m_latestVersion
                                                                      : m_failedDownloadVersion);
        return;
    }
    // An automatic install's error: retried the same way, so no restart now
    apply(m_errorFromAuto && m_autoUpdate && canStage() ? ApplyIntent::Auto
                                                        : ApplyIntent::Manual);
}

void UpdateManager::onVerifyDone(Outcome outcome, const QString &reason)
{
    if (m_state != Verifying)
        return;
    switch (outcome) {
    case Outcome::Ok:
        m_readyError.clear();
        startApply();
        break;
    case Outcome::Transport:
        // A package with an error (the user's Retry) keeps it in sight
        if (m_applyIntent == ApplyIntent::Auto && m_readyError.isEmpty()) {
            // Often no network yet (startup): onNetworkFound tries again
            setState(ReadyToInstall);
            break;
        }
        // No error of the package: the toggle may retry it (networkFound)
        m_lastError = reason;
        setState(NotVerified);
        break;
    case Outcome::Refused:
        // The package is fine; never "install anyway" past such an answer
        m_readyError = reason;
        m_errorFromAuto = m_applyIntent == ApplyIntent::Auto;
        backToReady();
        break;
    case Outcome::Unpublished: // the feed is about another version now
    case Outcome::NotListed:
    case Outcome::Mismatch:
        discardAndRedownload(reason);
        break;
    case Outcome::Local:
        m_readyVersion.clear();
        m_readyError.clear();
        setFailed(reason);
        break;
    }
}

void UpdateManager::discardAndRedownload(const QString &reason)
{
    // Feed and checksums no longer confirm it: remove it with its partial download
    const QString version = m_readyVersion;
    if (!version.isEmpty()) // cacheDir("") is updates/ itself
        QDir(cacheDir(version)).removeRecursively();
    m_readyVersion.clear();
    m_readyError.clear();
    m_verifiedVersion.clear();
    if (m_redownloaded.contains(version)) {
        setFailed(reason);
        return;
    }
    m_redownloaded.insert(version); // once per session: no loop
    m_applyAfterDownload = m_applyIntent == ApplyIntent::Manual;
    startDownload(version, m_applyAfterDownload);
}

bool UpdateManager::helperPending()
{
    // After the exit no lock guards the helper: only its pid file says it
    // still runs. An orphan (helper killed, crashed, never started) goes now
    const QString script = updatesFile(UpdateInstall::helperScriptName);
    const QString pidFile = updatesFile(UpdateInstall::helperPidName);
    if (QFileInfo::exists(script)) {
        QFile f(pidFile);
        const QList<QByteArray> parts = f.open(QIODevice::ReadOnly)
                ? f.readAll().trimmed().split(' ') : QList<QByteArray>();
        // The token, not the pid alone: pids are reused, e.g. after a reboot
        const qint64 pid = parts.value(0).toLongLong();
        if (parts.size() == 2 && !parts.at(1).isEmpty()
                && UpdateInstall::processStartToken(pid) == QLatin1String(parts.at(1)))
            return true;
    }
    QFile::remove(script);
    QFile::remove(pidFile); // the helper deletes only its script
    QFile::remove(updatesFile(UpdateInstall::relaunchRequestName));
    QFile::remove(updatesFile(UpdateInstall::journalName));
    UpdateWindows::clearRunOnce(m_config.installRoot);
    return false;
}

void UpdateManager::startApply()
{
    const QString pkg = updateFilePath(m_readyVersion);
    if (commitsAtExit()) {
        // The helper of an earlier exit still works on this install (quit, then
        // a quick relaunch): a new stage would rewrite or move what it uses
        if (helperPending()) {
            if (m_applyIntent == ApplyIntent::Auto) {
                setState(ReadyToInstall); // no error: networkFound tries again
                return;
            }
            refuseApply(QLatin1String("a previous update is still finishing"));
            return;
        }
        // Installs only when the last YAYC of this install quits (an AppImage
        // is safe: each runs its own mount). Said now, not after a restart
        if (m_applyIntent == ApplyIntent::Manual
                && m_config.updateMethod != UpdateConfig::ReplaceAppImage
                && UpdateInstall::othersRunning(updatesDir())) {
            refuseApply(QLatin1String("close the other YAYC windows of this installation first"));
            return;
        }
    }
    m_appliedThisSession = true;
    m_lastError.clear();
    setState(Applying);

    const QString staging = stagedPath();
    switch (updateMethod()) {
    case UpdateConfig::ReplaceAppImage:
        stageAppImage(pkg);
        break;
    case UpdateConfig::SwapDirectory:
        stageExtracted(QStringLiteral("tar"),
                       {QStringLiteral("-xf"), pkg, QStringLiteral("-C"), staging});
        break;
    case UpdateConfig::HelperSwap:
        // bsdtar reads zip (Windows 10 1803+). Not Expand-Archive: its
        // -DestinationPath takes [ ] as wildcards. By path, not by PATH
        stageExtracted(qEnvironmentVariable("SystemRoot") + QLatin1String("\\System32\\tar.exe"),
                       {QStringLiteral("-xf"), pkg, QStringLiteral("-C"), staging});
        break;
    case UpdateConfig::ElevatedDpkg:
        applyDeb(pkg);
        break;
    case UpdateConfig::Installer:
        stageDone(); // the helper runs it at exit
        break;
    case UpdateConfig::ManualDeb: {
        const QString sha256 = packageSha256();
        if (sha256.isEmpty()) {
            applyFailed(QLatin1String("cannot read update package: ") + pkg);
            break;
        }
        m_manualCommand = UpdateLinux::manualDebCommand(
                    pkg, sha256, QFileInfo(m_config.exePath).fileName());
        setState(ManualInstall); // shows the command the user runs
        break;
    }
    case UpdateConfig::OpenDmg:
        QDesktopServices::openUrl(QUrl::fromLocalFile(pkg));
        // Done with it. Idle keeps no ready package: one would block a
        // download of the same version, and so its Retry
        m_readyVersion.clear();
        m_readyError.clear();
        setState(Idle);
        break;
    case UpdateConfig::NoMethod:
        setFailed(QLatin1String("no update method available"));
        break;
    }
}

void UpdateManager::applyFailed(const QString &reason)
{
    m_readyError = reason;
    m_errorFromAuto = m_applyIntent == ApplyIntent::Auto;
    backToReady(); // retry() applies again
}

void UpdateManager::refuseApply(const QString &reason)
{
    m_errorFromAuto = false; // only a click gets here
    backToReady();
    m_lastError = reason; // shown with Retry, not kept as the package's error
    emit stateChanged();
}

bool UpdateManager::prepareRestart()
{
    if (m_state != ExitPending || m_config.updateMethod == UpdateConfig::ReplaceAppImage
            || !UpdateInstall::othersRunning(updatesDir()))
        return true;
    m_lastError = QLatin1String("close the other YAYC windows of this installation first");
    emit stateChanged();
    return false;
}

void UpdateManager::backToReady()
{
    m_failedDownloadVersion.clear();
    m_lastError = m_readyError;
    setState(m_autoStaged ? ExitPending : m_readyVersion.isEmpty() ? Idle : ReadyToInstall);
}

/*
    Apply
*/

void UpdateManager::stageDone()
{
    if (m_applyIntent == ApplyIntent::Auto && !m_autoUpdate) {
        dropAutoStage(); // the toggle went off while staging
        return;
    }
    m_autoStaged = m_applyIntent == ApplyIntent::Auto;
    setState(ExitPending);
    // The user clicked Apply: finish now
    if (m_applyIntent == ApplyIntent::Manual && restartsAfterStage(m_config.updateMethod))
        emit restartRequested();
}

void UpdateManager::dropAutoStage()
{
    if (m_state == Verifying)
        m_fetcher.cancel();
    else
        unstage();
    m_lastError = m_readyError;
    setState(ReadyToInstall);
}

QString UpdateManager::stagedPath() const
{
    if (m_config.updateMethod == UpdateConfig::ReplaceAppImage)
        return m_config.appImagePath + UpdateInstall::newSuffix;
    if (UpdateConfig::isDirSwap(m_config.updateMethod))
        return UpdateInstall::stagingDir(m_config.installRoot);
    return {};
}

void UpdateManager::unstage()
{
    QFile::remove(updatesFile(UpdateInstall::stageMarkerName));
    removePath(stagedPath());
    m_autoStaged = false;
}

void UpdateManager::stageAppImage(const QString &pkg)
{
    const QString target = m_config.appImagePath;
    // Next to the target: the rename at exit is atomic only within one filesystem
    const QString staged = stagedPath();
    // Off-thread: the copy takes the size of the image
    UpdateFetcher::runAsync(this, [pkg, target, staged, abort = m_stageAbort]() -> QString {
        QFile::remove(staged);
        if (!UpdateInstall::copyFile(pkg, staged, abort.get())) {
            QFile::remove(staged);
            return QLatin1String("cannot stage new AppImage next to the current one");
        }
        if (!QFile::setPermissions(staged, QFile::permissions(target) | QFileDevice::ExeOwner
                                   | QFileDevice::ExeGroup | QFileDevice::ExeUser)
                || !QFileInfo(staged).isExecutable()) {
            QFile::remove(staged);
            return QLatin1String("cannot make the new AppImage executable");
        }
        return {};
    }, [this](const QString &error) {
        if (!error.isEmpty())
            applyFailed(error);
        else
            stageDone();
    });
}

void UpdateManager::stageExtracted(const QString &program, const QStringList &args)
{
    const QString staging = stagedPath();
    const QString marker = updatesFile(UpdateInstall::stageMarkerName);
    QFile::remove(marker);
    QDir(staging).removeRecursively();
    if (!QDir().mkpath(staging)) {
        applyFailed(QLatin1String("cannot create staging directory"));
        return;
    }
    auto *proc = new QProcess(this);
    runProcess(proc, program, args,
               [this, proc, program, staging, marker](bool ok, const QString &startError) {
        // Checked now, so the commit at exit only renames. Windows: .exe is executable
        const QString exeName = QFileInfo(m_config.exePath).fileName();
        QString error;
        if (!startError.isEmpty())
            error = QLatin1String("cannot run ") + program + QLatin1String(": ") + startError;
        else if (!ok)
            error = QLatin1String("extraction failed: ")
                    + QString::fromUtf8(proc->readAllStandardError()).trimmed();
        else if (!QFileInfo(extractedRoot(staging) + QLatin1Char('/') + exeName).isExecutable())
            error = QLatin1String("extracted package does not contain ") + exeName;
        if (error.isEmpty()) {
            // Complete: resumeStage() may install it. Without the marker only
            // this session's exit does
            QFile f(marker);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
                f.write(m_readyVersion.toUtf8());
            stageDone();
            return;
        }
        QDir(staging).removeRecursively();
        applyFailed(error);
    });
}

QString UpdateManager::extractedRoot(const QString &stagingDir)
{
    // Archives wrap everything in one top-level dir; tolerate flat ones too
    const auto entries = QDir(stagingDir).entryInfoList(
                QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    if (entries.size() == 1 && entries.first().isDir())
        return entries.first().absoluteFilePath();
    return stagingDir;
}

void UpdateManager::applyDeb(const QString &pkg)
{
    const QString sha256 = packageSha256();
    if (sha256.isEmpty()) {
        applyFailed(QLatin1String("cannot read update package: ") + pkg);
        return;
    }
    // Files, not pipes: dpkg may outlive this process (see ~UpdateManager),
    // and a write to a closed pipe would kill it mid-install
    const QString log = updatesFile(dpkgLogName);
    auto *proc = new QProcess(this);
    m_rootProcess = proc;
    m_applyCancelled = false;
    emit stateChanged(); // canCancel
    proc->setStandardInputFile(QProcess::nullDevice());
    proc->setStandardOutputFile(QProcess::nullDevice());
    proc->setStandardErrorFile(log);
    // pkexec only: it can show its own GUI auth prompt, sudo cannot
    runProcess(proc, m_config.pkexecPath, UpdateLinux::debInstallCommand(pkg, sha256),
               [this, proc, log, pkg, sha256](bool ok, const QString &startError) {
        m_rootProcess = nullptr;
        if (!startError.isEmpty()) {
            applyFailed(QLatin1String("cannot run pkexec: ") + startError);
            return;
        }
        // The user's Cancel reached it before the password: no error. As
        // root it ignored the signal, and a dpkg failure is a failure
        if (!ok && m_applyCancelled && proc->exitStatus() == QProcess::CrashExit) {
            m_verifiedAt.invalidate();
            backToReady();
            return;
        }
        // 127: pkexec could not ask at all (no polkit agent: i3, sway), and
        // never will this session. The user runs the command instead.
        // 126, a dismissed prompt, is a plain failure below
        if (proc->exitStatus() == QProcess::NormalExit && proc->exitCode() == 127) {
            m_pkexecUnusable = true;
            m_manualCommand = UpdateLinux::manualDebCommand(
                        pkg, sha256, QFileInfo(m_config.exePath).fileName());
            setState(ManualInstall);
            return;
        }
        if (!ok) {
            QFile f(log);
            const QString err = f.open(QIODevice::ReadOnly)
                    ? QString::fromUtf8(f.readAll()).trimmed().right(500) : QString();
            // Also a cancelled password prompt: the package stays on offer. The
            // next try checks it again; its hash stays for Install anyway
            m_verifiedAt.invalidate();
            applyFailed(QLatin1String("install failed: ") + err);
            return;
        }
        setState(RestartPending);
        emit restartRequested(); // dpkg replaced libraries the process still loads
    });
}

void UpdateManager::runProcess(QProcess *proc, const QString &program, const QStringList &args,
                               std::function<void(bool, const QString &)> done)
{
    // Context this: ~UpdateManager disconnects a process it leaves running
    connect(proc, &QProcess::finished, this,
            [proc, done](int exitCode, QProcess::ExitStatus status) {
        proc->deleteLater();
        done(status == QProcess::NormalExit && exitCode == 0, QString());
    });
    connect(proc, &QProcess::errorOccurred, this, [proc, done](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return; // finished() reports the rest
        proc->deleteLater();
        done(false, proc->errorString());
    });
    proc->start(program, args);
}

/*
    Startup cleanup: backups from the previous (successful) update, staging
    leftovers, cached packages no newer than the running version.
*/

void UpdateManager::cleanupLeftovers()
{
    if (!updateSupported())
        return;

    // Only the owner: another instance's stage is not a leftover
    if (!ensureOwner())
        return;

    // Backup and staged copy of an earlier session. They stay when this process
    // applied (its way back), a helper still has to swap, or the live install
    // is broken. <root>.new (a failed rollback) and a .old the updater did not
    // make always stay
    if (canStage() && !m_appliedThisSession) {
        const bool appImage = m_config.updateMethod == UpdateConfig::ReplaceAppImage;
        const QString liveExe = appImage ? m_config.appImagePath : m_config.exePath;
        const QString target = appImage ? m_config.appImagePath : m_config.installRoot;
        if (!helperPending() && UpdateInstall::isSafeToRemoveBackup(liveExe)) {
            // In the live tree: a .old moved back by hand. A copy of it would
            // pass for the updater's. Not while a helper swaps: it put it there
            if (!appImage)
                QFile::remove(UpdateInstall::backupMarker(target, true));
            // The way back stays until this version ran well once (markGoodStart)
            if (ranWell())
                UpdateInstall::removeBackup(target + UpdateInstall::oldSuffix);
            QFile::remove(updatesFile(UpdateInstall::stageMarkerName));
            removePath(stagedPath());
        }
    }

    QDir updates(updatesDir());
    const auto current = QVersionNumber::fromString(m_config.currentVersion);
    const auto entries = updates.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        const auto v = QVersionNumber::fromString(entry);
        if (!v.isNull() && v <= current)
            QDir(updates.filePath(entry)).removeRecursively();
    }
}

void UpdateManager::markGoodStart()
{
    QFile f(updatesFile(UpdateInstall::goodStartName));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(m_config.currentVersion.toUtf8());
}

bool UpdateManager::ranWell() const
{
    QFile f(updatesFile(UpdateInstall::goodStartName));
    return f.open(QIODevice::ReadOnly)
            && QString::fromUtf8(f.readAll()).trimmed() == m_config.currentVersion;
}

void UpdateManager::cancelDownload()
{
    if (m_state == Applying && m_rootProcess) {
        // Before the password pkexec is the user's process; as root it ignores this
        m_applyCancelled = true;
        m_rootProcess->terminate();
        return;
    }
    // The user's "no" to automatic work: not redone this session
    if (m_state == Verifying && m_applyIntent == ApplyIntent::Auto)
        m_declinedVersion = m_readyVersion;
    else if (m_state == Downloading && !m_downloadByUser)
        m_declinedVersion = m_downloadVersion;
    stopWork();
}

void UpdateManager::withdrawStage()
{
    if (m_state != ExitPending)
        return;
    if (m_autoStaged)
        m_declinedVersion = m_readyVersion; // else the toggle stages it again
    unstage();
    m_failedDownloadVersion.clear();
    m_lastError.clear();
    setState(ReadyToInstall);
}

void UpdateManager::stopWork()
{
    if (m_state != Downloading && m_state != Verifying)
        return;
    m_fetcher.cancel(); // .part stays: the next attempt resumes from it
    m_applyAfterDownload = false; // the Apply it carried is cancelled too
    m_downloadVersion.clear();
    backToReady();
}
