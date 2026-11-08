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

#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include "UpdateConfig.h"
#include "UpdateFetcher.h"
#include "UpdateInstall.h"

#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <memory>

class QNetworkReply;
class QLockFile;
class QProcess;

// The update state machine: check, download, verify, stage; the exit plan
// finishes the install (UpdateInstall)
class UpdateManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool updateSupported READ updateSupported CONSTANT)
    // Why not supported (English, like lastError)
    Q_PROPERTY(QString unsupportedReason READ unsupportedReason CONSTANT)
    // ElevatedDpkg turns ManualDeb once pkexec cannot ask (no polkit agent)
    Q_PROPERTY(UpdateConfig::Method updateMethod READ updateMethod NOTIFY stateChanged)
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    // The "Automatic updates" toggle: download, verify and stage by itself,
    // then install when YAYC quits
    Q_PROPERTY(bool autoUpdate READ autoUpdate WRITE setAutoUpdate NOTIFY autoUpdateChanged)
    // False: the toggle only downloads (password prompt, or a manual method)
    Q_PROPERTY(bool canStage READ canStage CONSTANT)
    Q_PROPERTY(UpdateState state READ state NOTIFY stateChanged)
    Q_PROPERTY(qreal downloadProgress READ downloadProgress NOTIFY downloadProgressChanged) // 0..1
    // All below change only together with a state change
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)
    Q_PROPERTY(QString readyVersion READ readyVersion NOTIFY stateChanged)
    Q_PROPERTY(QString downloadVersion READ downloadVersion NOTIFY stateChanged)
    // The release has no package for this install (e.g. Windows only)
    Q_PROPERTY(QString unavailableVersion READ unavailableVersion NOTIFY stateChanged)
    Q_PROPERTY(bool canRetry READ canRetry NOTIFY stateChanged)
    // NotVerified: "Install anyway" is offered
    Q_PROPERTY(bool canInstallUnverified READ canInstallUnverified NOTIFY stateChanged)
    // cancelDownload() stops something: a download, a verify, a password prompt
    Q_PROPERTY(bool canCancel READ canCancel NOTIFY stateChanged)
    // The ready package is staged by the toggle; a newer one may still download
    Q_PROPERTY(bool autoStaged READ autoStaged NOTIFY stateChanged)
    // Retry repeats a failed download (else it repeats the install)
    Q_PROPERTY(bool retryDownloads READ retryDownloads NOTIFY stateChanged)
    // ManualInstall only: the command the user runs to install the ready
    // package and close the running (outdated) instance
    Q_PROPERTY(QString manualInstallCommand READ manualInstallCommand NOTIFY stateChanged)
    // Another YAYC of this install does the update work; this one only checks
    Q_PROPERTY(bool updatesElsewhere READ updatesElsewhere NOTIFY stateChanged)

public:
    enum UpdateState {
        Idle = 0,
        Downloading,      // downloadVersion in flight; readyVersion (if any) stays,
                          // staged too if it was by itself
        Applying,
        ReadyToInstall,   // readyVersion downloaded + verified; waits for the user
        RestartPending,   // deb: installed by dpkg; restart to run the new version
        ExitPending,      // staged; installed when the app exits (finishOnExit)
        Failed,
        Verifying,        // re-checking readyVersion against fresh feed and checksums
        NotVerified,      // re-check hit a network error: retry, or install anyway
        ManualInstall     // manual-deb: the user runs manualInstallCommand
    };
    Q_ENUM(UpdateState)

    explicit UpdateManager(QObject *parent = nullptr); // UpdateConfig::detect()
    explicit UpdateManager(const UpdateConfig &config, QObject *parent = nullptr);
    ~UpdateManager() override;

    UpdateConfig::InstallType installType() const;
    bool updateSupported() const;
    QString unsupportedReason() const;
    UpdateConfig::Method updateMethod() const;
    bool checking() const;
    bool autoUpdate() const;
    void setAutoUpdate(bool on);
    // Methods that need no password or UAC: stage now, commit at exit
    bool canStage() const;
    UpdateState state() const;
    qreal downloadProgress() const;
    QString lastError() const;
    QString readyVersion() const;
    QString downloadVersion() const;
    QString unavailableVersion() const;
    bool canRetry() const;
    bool canInstallUnverified() const;
    bool canCancel() const;
    bool autoStaged() const;
    bool retryDownloads() const;
    QString manualInstallCommand() const;
    bool updatesElsewhere() const;

    // Offline pre-check: a fully downloaded (non-.part) package for
    // \a version sits at the expected cache path. applyUpdate() re-checks
    // it against fresh feed and checksums before installing.
    bool isUpdateDownloaded(const QString &version) const;
    QString updateFilePath(const QString &version) const;

    // Fetches the latest version. Always ends with latestVersionFound (then
    // upToDate when not newer) or checkFailed. Toggle on: downloads a newer one
    Q_INVOKABLE void checkForUpdates();
    // Download + verify, then stop at ReadyToInstall: applying is always a
    // user action (applyUpdate). A failure goes back to an already ready
    // package, else to Failed.
    Q_INVOKABLE void startUpdate(const QString &version);
    // Re-verifies the ready package against fresh feed and checksums, then applies
    // it (manual-deb: shows the install command)
    Q_INVOKABLE void applyUpdate();
    // NotVerified only (see canInstallUnverified): install without the re-check
    Q_INVOKABLE void applyUnverified();
    // Toggle on: verify and stage the ready package by itself. Safe to call
    // any time (networkFound does, so a startup without network catches up)
    void startAutoInstall();
    // After a failed download or apply (see canRetry); in NotVerified, the
    // re-check
    Q_INVOKABLE void retry();
    // Startup, offline: the newest package newer than this build is ready again
    // (a final name means its checksum matched). If its install failed at the
    // last exit, that error shows and blocks the automatic install
    void resumeDownloaded();
    // Startup, before the UI: a complete stage an earlier session left for
    // its exit (Windows portable). True: it is ExitPending; quit now with a
    // restart, the helper installs it and relaunches
    bool resumeStage();
    // Startup, first: the Windows helper of an earlier exit still works on
    // this install, and needs its files free. True: quit now; the helper
    // starts YAYC once done
    bool deferToHelper();
    // Downloading or Verifying (.part stays), or pkexec still asking for the
    // password. Automatic work the user stops is not redone this session
    Q_INVOKABLE void cancelDownload();
    // ExitPending: the stage goes, the package is on offer again. Not redone
    // by itself this session
    Q_INVOKABLE void withdrawStage();
    // Before "Restart now": false if the exit would not install (other YAYC
    // windows of this install run); lastError then says so
    Q_INVOKABLE bool prepareRestart();

    // This version ran a while with its window up: the next session may
    // delete the .old of the version before (main.cpp, after 5 minutes)
    void markGoodStart();

    UpdateInstall::ExitPlan takeExitPlan();
    // UpdateInstall::runExitPlan(takeExitPlan(), restart)
    bool finishOnExit(bool restart = false);

    // Strict X.Y.Z: the version goes into file paths and URLs
    static bool isValidVersion(const QString &version);

signals:
    void checkingChanged();
    void autoUpdateChanged();
    void stateChanged();
    void downloadProgressChanged();
    void latestVersionFound(const QString &version);
    void upToDate();
    void checkFailed(const QString &reason);
    // Applied over files the running process still loads: restart now
    void restartRequested();

private:
    using Outcome = UpdateFetcher::Outcome;
    // Auto: staged by itself, waits for the quit. Manual: the user clicked
    // Apply, so restart now
    enum class ApplyIntent { Manual, Auto };

    void setState(UpdateState state);
    void setFailed(const QString &reason);
    void setDownloadProgress(qreal progress);
    QString cacheDir(const QString &version) const;

    // Download, or only verify, the package of \a version
    void fetch(const QString &version, bool verifyOnly, UpdateFetcher::Done done);
    // startUpdate(); \a byUser: a click, not the automatic check
    void requestDownload(const QString &version, bool byUser);
    void startDownload(const QString &version, bool byUser);
    void onDownloadDone(Outcome outcome, const QString &reason);
    void onVerifyDone(Outcome outcome, const QString &reason);
    void discardAndRedownload(const QString &reason);
    // What last-install-error.json says about \a version; empty: nothing
    QString installErrorFor(const QString &version) const;
    void markVerified(const QString &version, const QString &sha256);
    bool isFreshlyVerified() const;
    // The SHA-256 a root or admin install must find: the one feed and
    // checksums gave this session. Empty: none
    QString packageSha256() const;
    // Takes the owner lock if free: the one instance of this install that
    // downloads, stages and cleans up. False: another holds it (updatesElsewhere)
    bool ensureOwner();
    // canStage(), plus the installer: the methods that stage for the exit
    bool commitsAtExit() const;
    void beginVerify();
    void apply(ApplyIntent intent); // from ReadyToInstall: verify, then stage
    void startApply();
    // Cancel without a user's "no": the toggle went off
    void stopWork();
    // The install on disk is unchanged: the package is offered again
    void applyFailed(const QString &reason);
    // Not now (other windows, a helper): \a reason shows, nothing is blocked
    void refuseApply(const QString &reason);
    // The ready package (if any) is on offer again, with only its own error
    void backToReady();

    void stageDone();
    // What a stage writes: <AppImage>.new, or <installRoot>.staging. Empty: nothing
    QString stagedPath() const;
    void unstage();
    // Undoes an automatic verify or stage. Never while a stage runs: callers check
    void dropAutoStage();
    void stageAppImage(const QString &pkg);
    // Runs the extractor into stagedPath(); the tree is swapped in at exit
    void stageExtracted(const QString &program, const QStringList &args);
    void applyDeb(const QString &pkg);
    // Calls \a done once (ok: exit code 0; startError: never started), then
    // deletes \a proc
    void runProcess(QProcess *proc, const QString &program, const QStringList &args,
                    std::function<void(bool ok, const QString &startError)> done);
    static QString extractedRoot(const QString &stagingDir);
    // Windows helper of an earlier exit still running. Removes an orphaned
    // script and its relaunch request
    bool helperPending();
    void cleanupLeftovers();
    bool ranWell() const; // markGoodStart() for this version, any session

    QString updatesDir() const;
    QString updatesFile(const QString &name) const;

    QNetworkAccessManager m_nam;
    QNetworkReply *m_checkReply{nullptr};
    const UpdateConfig m_config;
    bool m_autoUpdate{false};
    ApplyIntent m_applyIntent{ApplyIntent::Manual};
    UpdateState m_state{Idle};
    qreal m_downloadProgress{0};
    QString m_lastError;          // shown; may be a newer download's
    QString m_readyError;         // of the ready package: blocks the automatic install
    bool m_errorFromAuto{false};  // m_readyError came from automatic work: Retry stays Auto
    QString m_declinedVersion;    // the user cancelled automatic work on it this session
    QString m_readyVersion;       // verified package on disk; can be applied
    QString m_downloadVersion;    // download in flight
    bool m_downloadByUser{false}; // that download: a click, not the automatic check
    QString m_latestVersion;      // newest release known this session
    QString m_failedDownloadVersion; // retry() downloads it again
    QString m_unavailableVersion; // no package for this install in that release
    QSet<QString> m_redownloaded; // one fresh download per version after a mismatch
    bool m_applyAfterDownload{false}; // a manual Apply waits for that re-download
    QString m_verifiedVersion;    // last package checked against fresh feed and checksums
    QString m_verifiedSha256;     // the SHA-256 both agreed on
    QString m_manualCommand;      // ManualInstall: built once, as hashing may block
    QElapsedTimer m_verifiedAt;
    // Held from the first update work until the exit plan is done
    std::unique_ptr<QLockFile> m_ownerLock;
    bool m_updatesElsewhere{false};
    // readyVersion is staged by itself and installs at exit. Stays while a
    // newer one downloads, until that one is ready
    bool m_autoStaged{false};
    // Set on destruction: stops a staging copy still running at quit
    std::shared_ptr<std::atomic_bool> m_stageAbort{std::make_shared<std::atomic_bool>(false)};
    // This process started an apply: its backups are the only way back
    bool m_appliedThisSession{false};
    QProcess *m_rootProcess{nullptr}; // pkexec dpkg while it runs
    bool m_applyCancelled{false};     // Cancel sent to it
    bool m_pkexecUnusable{false};     // it exited 127: ManualDeb from now on
    UpdateFetcher m_fetcher{m_nam, this, [this](qreal p) { setDownloadProgress(p); }};
};

#endif // UPDATEMANAGER_H
