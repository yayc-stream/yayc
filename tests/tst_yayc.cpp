#include <QtTest>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QSettings>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "YaycUtilities.h"
#include "UpdateLinux.h"
#include "UpdateManager.h"
#include "UpdateWindows.h"
#include "SingleInstance.h"
#include "TestControl.h"
#include "TestHttpServer.h"

#include <cstdio>
#include <memory>

#ifdef Q_OS_UNIX
#include <csignal>
#include <sys/stat.h>
#include <unistd.h>
#endif

// Takes what QDesktopServices::openUrl() would open
class UrlCatcher : public QObject
{
    Q_OBJECT
public:
    QList<QUrl> urls;
public slots:
    void open(const QUrl &url) { urls.append(url); }
};

class TestYayc : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void compareSemver_data();
    void compareSemver();

    void runAsync_threads();
    void runAsync_contextDeleted();
    void sha256File();

    void isValidVersion_data();
    void isValidVersion();
    void shellQuote_data();
    void shellQuote();
    void selfBuiltNeverUpdates();
    void applyUpdateNeedsReadyPackage();
    void isListedByDpkg();
    void sigtermRequestsQuit();
    void quitSignalRestoresPreviousHandler();
    void isSafeToRemoveBackup();
    void isYaycDir();
    void readManifest();
    void writeBytesUntranslated();
    void carryOverDirs();
    void assetNames_data();
    void assetNames();
    void copyFile();
    void runningAppImage_data();
    void runningAppImage();
    void partMeta();
    void parseUpdateFeed_data();
    void parseUpdateFeed();
    void windowsHelperScript();
    void debInstallCommand();
    void commitAppImage();
    void commitDirSwap();
    void backupMarkerInLiveTree();
    void backupPartlyRemoved();
    void e2e_retryOfflineKeepsError();
    void e2e_restartWithOthers();
    void updatesDirPerInstall();
    void processStartToken();
    void testControl();

    // End to end: tarball method, local server, temp dirs
    void e2e_checkForUpdates();
    void e2e_checkStartsDownload();
    void e2e_manualFlow();
    void e2e_reverifyAfterRestart();
    void e2e_resume_data();
    void e2e_resume();
    void e2e_interruptedDownload();
    void e2e_stallTimeout();
    void e2e_downloadMismatch();
    void e2e_verifyMismatchRedownload();
    void e2e_notVerified();
    void e2e_notVerifiedAutoRetry();
    void e2e_assetMissingRetried();
    void e2e_retryKeepsAutoIntent();
    void e2e_cancelDeclinesAuto();
    void e2e_withdrawStage();
    void e2e_cancelPasswordPrompt();
    void e2e_autoOfflineThenOnline();
    void e2e_toggleOffDuringStage();
    void e2e_commitErrorPersisted();
    void e2e_secondInstancePassive();
    void e2e_downloadSecondInstance();
    void othersRunning();
    void e2e_commitWaitsForOthers();
    void deferToHelper();
    void singleInstance();
    void e2e_cancelRedownloadDropsApply();
    void e2e_verifyRefused();
    void e2e_retryAfterRefusedVerify();
    void e2e_freshWindowDoesNotSlide();
    void e2e_cancelReverifyKeepsError();
    void e2e_newerReadyDropsOlder();
    void e2e_carryOverUserFiles();
    void e2e_partialRelease();
    void e2e_unpublishedRelease();
    void e2e_newerDownloadKeepsReadyError();
    void e2e_resumeNewestDownloaded();
    void e2e_unavailableNewerKeepsReadyClean();
    void e2e_newerReplacesAutoStage();
    void e2e_autoStageSurvivesNewerFailure();
    void e2e_clickFollowsFeed();
    void e2e_openDmgThenRetry();
    void e2e_retryTakesLatest();
    void e2e_feedDisagreesWithSums();
    void e2e_feedBehindAppVersion();
    void e2e_feedUnreadable_data();
    void e2e_feedUnreadable();
    void e2e_feedUnreadableAtVerify_data();
    void e2e_feedUnreadableAtVerify();
    void e2e_noChecksumsInFeed();
    void e2e_feedNewerThanReady();
    void cleanupWaitsForHelper();
    void cleanupAppImageLeftovers();
    void e2e_carryOverFailureKeepsInstall();
    void e2e_userBackupKept();
    void e2e_twoInstallsSameParent();
    void e2e_helperScriptAtExit();
    void e2e_resumeStage();
    void helperSwapNeedsStagedExe();
    void e2e_helperPendingBlocksStage();
    void e2e_orphanHelperIgnored();
    void e2e_extractFailures();
    void e2e_manualDeb();
    void e2e_pkexecWithoutAgent();

private:
    // Package, built once
    QTemporaryDir m_pkgDir;
    QByteArray m_pkg;
    bool m_e2eReady{false};
    // Per test
    std::unique_ptr<QTemporaryDir> m_tmp;
    std::unique_ptr<TestHttpServer> m_server;

    static constexpr const char *asset = "yayc-1.1.0-linux-x86_64.tar.xz";
    static QByteArray assetName(const QByteArray &version)
    {
        return "yayc-" + version + "-linux-x86_64.tar.xz";
    }
    QByteArray pkgSha256() const
    {
        return QCryptographicHash::hash(m_pkg, QCryptographicHash::Sha256).toHex();
    }
    QByteArray feedPath() const { return "/update/latest.json"; }
    QByteArray sumsPath() const { return "/releases/download/yayc-v1.1.0/SHA256SUMS"; }
    QByteArray assetPath() const { return QByteArray("/releases/download/yayc-v1.1.0/") + asset; }
    QString installRoot() const { return m_tmp->filePath("apps/yayc"); }
    QString stagingDir() const { return UpdateInstall::stagingDir(installRoot()); }
    QString cached() const { return m_tmp->filePath(QStringLiteral("cache/updates/1.1.0/") + asset); }
    QString installErrorFile() const { return m_tmp->filePath("cache/updates/last-install-error.json"); }
    // The updater's marker in <installRoot>.old
    QString backupMarker() const { return installRoot() + ".old/.yayc-backup"; }
    // markGoodStart() of the running version (1.0.0): cleanup may delete .old
    bool markRanWell();
    // Release <version> on the server, with the same package bytes, and the
    // feed about it. ETag "v1": the first upload of an asset, as the resume
    // tests' .part.meta says
    void addRelease(const QByteArray &version);
    // The update feed about release <version>: our package (or \a name),
    // with \a sha256Hex
    QJsonObject feed(const QByteArray &version, const QByteArray &sha256Hex,
                     const QByteArray &name = {}) const;
    void setFeed(const QJsonObject &feed);
    // Release 1.1.0 serves \a bytes as package \a name, in feed and SHA256SUMS
    void servePackage(const QByteArray &name, const QByteArray &bytes);
    // Release <version> without a package for this install, in feed and SHA256SUMS
    void unlistPackage(const QByteArray &version);
    // The feed of 1.1.0 cannot be read: \a how is a row of e2e_feedUnreadable
    void breakFeed(const QByteArray &how);
    UpdateConfig e2eConfig() const;
    QString helperScript() const { return m_tmp->filePath("cache/updates/yayc-update.cmd"); }
    QString helperPid() const { return m_tmp->filePath("cache/updates/yayc-update.pid"); }
    // A Windows helper left by an earlier exit, run by process \a pid
    bool writeHelper(qint64 pid, const QByteArray &token);
    bool writeLiveHelper(); // run by this process
    static qint64 deadPid()
    {
        QProcess child;
        child.start("sleep", {"30"});
        child.waitForStarted();
        const qint64 pid = child.processId();
        child.kill();
        child.waitForFinished();
        return pid;
    }
    // Feed and SHA256SUMS of 1.1.0 give \a sha256Hex for our package
    void setSums(const QByteArray &sha256Hex);
    // A first session that leaves a verified package in the cache
    bool downloadInFirstSession();
    // Download and Apply 1.1.0, up to ExitPending
    static bool stageInSession(UpdateManager &um);
};

namespace {
bool writeFile(const QString &path, const QByteArray &data, bool executable)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(data) != data.size())
        return false;
    f.close();
    return !executable || f.setPermissions(f.permissions() | QFileDevice::ExeOwner);
}

QByteArray readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// A file in the update cache, its folder created
bool seedPackage(const QString &path, const QByteArray &data)
{
    return QDir().mkpath(QFileInfo(path).path()) && writeFile(path, data, false);
}

// A SHA256SUMS line
QByteArray sumsLine(const QByteArray &data, const QByteArray &name)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex() + "  " + name + "\n";
}

// An update feed about 1.1.0, as yayc-ci writes it; \a edits replace its @tokens@
QByteArray feedJson(const QList<std::pair<QByteArray, QByteArray>> &edits = {})
{
    QByteArray json = R"({"schema": @SCHEMA@, "tag": "@TAG@", "version": "@VERSION@",
  "date": "2026-11-08T00:00:00Z", "url": "https://example.org/r", "future": {"x": [1]},
  "checksums_url": @CHECKSUMS@,
  "assets": [
    {"name": "yayc-v1.1.0-win-x64.exe", "url": "https://example.org/r/win.exe", "size": 1,
     "sha256": null},
    {"name": "yayc-1.1.0-x86_64.AppImage", "url": @URL@, "size": 10, "sha256": @SHA@,
     "extra": true}]})";
    QList<std::pair<QByteArray, QByteArray>> tokens{
        {"@SCHEMA@", "1"},
        {"@TAG@", "yayc-v1.1.0"},
        {"@VERSION@", "1.1.0"},
        {"@CHECKSUMS@", "\"https://example.org/r/yayc-v1.1.0/SHA256SUMS\""},
        {"@URL@", "\"https://example.org/r/yayc-v1.1.0/yayc-1.1.0-x86_64.AppImage\""},
        {"@SHA@", '"' + QByteArray(64, 'a') + '"'}};
    for (const auto &[token, value] : edits)
        for (auto &t : tokens)
            if (t.first == token)
                t.second = value;
    for (const auto &[token, value] : std::as_const(tokens))
        json.replace(token, value);
    return json;
}
}

#ifdef Q_OS_UNIX
namespace {
void dummySigtermHandler(int) {}
}
#endif

void TestYayc::initTestCase()
{
    // The production constructor (SelfBuilt tests) must not touch ~/.cache
    QStandardPaths::setTestModeEnabled(true);
#ifdef Q_OS_UNIX
    if (QStandardPaths::findExecutable("tar").isEmpty() || QStandardPaths::findExecutable("xz").isEmpty())
        return;
    // yayc-1.1.0/{yayc, data.bin}: incompressible data, so splits are real
    const QString tree = m_pkgDir.filePath("yayc-1.1.0");
    QVERIFY(QDir().mkpath(tree));
    QVERIFY(writeFile(tree + "/yayc", "new", true));
    QByteArray data(64 * 1024, Qt::Uninitialized);
    for (char &c : data)
        c = char(QRandomGenerator::global()->generate());
    QVERIFY(writeFile(tree + "/data.bin", data, false));
    const QString pkg = m_pkgDir.filePath(asset);
    if (QProcess::execute("tar", {"-cJf", pkg, "-C", m_pkgDir.path(), "yayc-1.1.0"}) != 0)
        return;
    m_pkg = readFile(pkg);
    m_e2eReady = !m_pkg.isEmpty();
#endif
}

void TestYayc::init()
{
    m_tmp = std::make_unique<QTemporaryDir>();
    QVERIFY(m_tmp->isValid());
    QVERIFY(QDir().mkpath(installRoot()));
    QVERIFY(writeFile(installRoot() + "/yayc", "old", true));

    m_server = std::make_unique<TestHttpServer>();
    QVERIFY(m_server->isListening());
    m_server->routes.insert("/APPVERSION", {.body = "1.1.0\n"});
    addRelease("1.1.0");
}

void TestYayc::cleanup()
{
    m_server.reset();
    m_tmp.reset();
}

UpdateConfig TestYayc::e2eConfig() const
{
    UpdateConfig c;
    c.installType = UpdateConfig::LinuxTarball;
    c.updateMethod = UpdateConfig::SwapDirectory;
    c.releasesUrl = m_server->url("/releases/download");
    c.latestVersionUrl = m_server->url("/APPVERSION");
    c.updateFeedUrl = m_server->url(feedPath());
    c.currentVersion = QStringLiteral("1.0.0");
    c.arch = QStringLiteral("x86_64");
    c.exePath = installRoot() + "/yayc";
    c.installRoot = installRoot();
    c.updatesDir = m_tmp->filePath("cache/updates");
    c.transferTimeoutMs = 500;
    c.cleanupDelayMs = -1;
    c.shippedFiles = {QStringLiteral("yayc"), QStringLiteral("libold.so")};
    return c;
}

bool TestYayc::writeHelper(qint64 pid, const QByteArray &token)
{
    return QDir().mkpath(QFileInfo(helperScript()).path())
            && writeFile(helperScript(), "rem", false)
            && writeFile(helperPid(), QByteArray::number(pid) + ' ' + token, false);
}

bool TestYayc::markRanWell()
{
    return writeFile(m_tmp->filePath("cache/updates/good-start"), "1.0.0", false);
}

bool TestYayc::writeLiveHelper()
{
    const qint64 self = QCoreApplication::applicationPid();
    return writeHelper(self, UpdateInstall::processStartToken(self).toLatin1());
}

void TestYayc::addRelease(const QByteArray &version)
{
    const QByteArray name = assetName(version);
    const QByteArray dir = "/releases/download/yayc-v" + version + '/';
    m_server->routes.insert(dir + name, {.body = m_pkg, .etag = "\"v1\""});
    m_server->routes.insert(dir + "SHA256SUMS", {.body = sumsLine(m_pkg, name)});
    setFeed(feed(version, pkgSha256()));
}

QJsonObject TestYayc::feed(const QByteArray &version, const QByteArray &sha256Hex,
                           const QByteArray &name) const
{
    const QByteArray dir = "/releases/download/yayc-v" + version + '/';
    const QByteArray file = name.isEmpty() ? assetName(version) : name;
    const QJsonObject package{{"name", QString::fromLatin1(file)},
                              {"url", m_server->url(dir + file)},
                              {"size", m_pkg.size()},
                              {"sha256", QString::fromLatin1(sha256Hex)}};
    return {{"schema", 1},
            {"tag", QString::fromLatin1("yayc-v" + version)},
            {"version", QString::fromLatin1(version)},
            {"checksums_url", m_server->url(dir + "SHA256SUMS")},
            {"assets", QJsonArray{package}}};
}

void TestYayc::setFeed(const QJsonObject &feed)
{
    m_server->routes.insert(feedPath(), {.body = QJsonDocument(feed).toJson()});
}

void TestYayc::servePackage(const QByteArray &name, const QByteArray &bytes)
{
    m_server->routes.insert("/releases/download/yayc-v1.1.0/" + name,
                            {.body = bytes, .etag = "\"v1\""});
    m_server->routes.insert(sumsPath(), {.body = sumsLine(bytes, name)});
    setFeed(feed("1.1.0", QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex(),
                 name));
}

void TestYayc::unlistPackage(const QByteArray &version)
{
    const QByteArray other = "yayc-v" + version + "-win-x64.exe";
    const QByteArray dir = "/releases/download/yayc-v" + version + '/';
    m_server->routes.insert(dir + "SHA256SUMS", {.body = QByteArray(64, 'a') + "  " + other + "\n"});
    QJsonObject f = feed(version, QByteArray(64, 'a'));
    f["assets"] = QJsonArray{QJsonObject{{"name", QString::fromLatin1(other)},
                                         {"url", m_server->url(dir + other)},
                                         {"sha256", QString(64, 'a')}}};
    setFeed(f);
}

void TestYayc::breakFeed(const QByteArray &how)
{
    if (how == "404" || how == "503") {
        m_server->routes[feedPath()].status = how.toInt();
    } else if (how == "not JSON") {
        m_server->routes[feedPath()].body = "<html>";
    } else { // a URL neither https nor on the test server
        QJsonObject f = feed("1.1.0", pkgSha256());
        f["checksums_url"] = "http://example.org/SHA256SUMS";
        setFeed(f);
    }
}

void TestYayc::setSums(const QByteArray &sha256Hex)
{
    m_server->routes.insert(sumsPath(), {.body = sha256Hex + "  " + asset + "\n"});
    setFeed(feed("1.1.0", sha256Hex));
}

bool TestYayc::downloadInFirstSession()
{
    UpdateManager first(e2eConfig());
    first.startUpdate("1.1.0");
    if (!QTest::qWaitFor([&]() { return first.state() == UpdateManager::ReadyToInstall; }, 5000))
        return false;
    return QFileInfo::exists(cached());
}

bool TestYayc::stageInSession(UpdateManager &um)
{
    um.startUpdate("1.1.0");
    if (!QTest::qWaitFor([&]() { return um.state() == UpdateManager::ReadyToInstall; }, 5000))
        return false;
    um.applyUpdate();
    return QTest::qWaitFor([&]() { return um.state() == UpdateManager::ExitPending; }, 5000);
}

#define REQUIRE_E2E() \
    if (!m_e2eReady) \
        QSKIP("needs tar + xz (Unix)")

void TestYayc::e2e_checkForUpdates()
{
    REQUIRE_E2E();
    UpdateManager um(e2eConfig());
    QSignalSpy found(&um, &UpdateManager::latestVersionFound);
    QSignalSpy upToDate(&um, &UpdateManager::upToDate);
    QSignalSpy failed(&um, &UpdateManager::checkFailed);

    um.checkForUpdates();
    QTRY_COMPARE(found.count(), 1);
    QCOMPARE(found.at(0).at(0).toString(), QStringLiteral("1.1.0"));
    QCOMPARE(upToDate.count(), 0);

    m_server->routes["/APPVERSION"].body = "1.0.0\n";
    um.checkForUpdates();
    QTRY_COMPARE(upToDate.count(), 1);

    m_server->routes["/APPVERSION"].status = 404;
    um.checkForUpdates();
    QTRY_COMPARE(failed.count(), 1);
    QVERIFY(!um.checking());
}

void TestYayc::e2e_checkStartsDownload()
{
    REQUIRE_E2E();
    {
        UpdateManager um(e2eConfig());
        QSignalSpy found(&um, &UpdateManager::latestVersionFound);
        um.checkForUpdates(); // toggle off: the check only reports
        QTRY_COMPARE(found.count(), 1);
        QTest::qWait(100);
        QCOMPARE(um.state(), UpdateManager::Idle);
        QCOMPARE(m_server->count(feedPath()), 0);
        QCOMPARE(m_server->count(sumsPath()), 0);

        um.setAutoUpdate(true);
        um.checkForUpdates(); // on: downloads, then stages for the quit
        QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
        um.setAutoUpdate(false);
    }
    // A release without a package for this install: asked once per session
    unlistPackage("1.1.0");
    UpdateManager um(e2eConfig());
    um.setAutoUpdate(true);
    QSignalSpy found(&um, &UpdateManager::latestVersionFound);
    um.checkForUpdates();
    QTRY_COMPARE(um.unavailableVersion(), QStringLiteral("1.1.0"));
    const int feeds = m_server->count(feedPath());
    um.checkForUpdates();
    QTRY_COMPARE(found.count(), 2);
    QTest::qWait(100);
    QCOMPARE(m_server->count(feedPath()), feeds);
}

void TestYayc::e2e_manualFlow()
{
    REQUIRE_E2E();
    UpdateManager um(e2eConfig());
    QSignalSpy restart(&um, &UpdateManager::restartRequested);
    um.startUpdate("1.1.0");
    QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
    QCOMPARE(readFile(cached()), m_pkg);

    // Verified moments ago: Apply fetches neither feed nor checksums again
    const int feeds = m_server->count(feedPath());
    const int checksums = m_server->count(sumsPath());
    um.applyUpdate();
    QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
    QCOMPARE(m_server->count(feedPath()), feeds);
    QCOMPARE(m_server->count(sumsPath()), checksums);
    QCOMPARE(restart.count(), 1); // a click: restart now

    // Nothing touched the live install until the exit
    QCOMPARE(readFile(installRoot() + "/yayc"), QByteArray("old"));
    QVERIFY(!um.finishOnExit());
    QCOMPARE(readFile(installRoot() + "/yayc"), QByteArray("new"));
    QCOMPARE(readFile(installRoot() + ".old/yayc"), QByteArray("old"));
    QVERIFY(QFileInfo::exists(backupMarker())); // the updater's: cleanup may delete it
    QVERIFY(!QFileInfo::exists(installRoot() + "/.yayc-backup"));
    QVERIFY(!QFileInfo::exists(stagingDir()));
}

void TestYayc::e2e_reverifyAfterRestart()
{
    REQUIRE_E2E();
    QVERIFY(downloadInFirstSession());
    UpdateManager second(e2eConfig());
    second.resumeDownloaded();
    QCOMPARE(second.state(), UpdateManager::ReadyToInstall);
    // From another session: always against fresh feed and checksums
    const int feeds = m_server->count(feedPath());
    const int checksums = m_server->count(sumsPath());
    second.applyUpdate();
    QTRY_COMPARE(second.state(), UpdateManager::ExitPending);
    QCOMPARE(m_server->count(feedPath()), feeds + 1);
    QCOMPARE(m_server->count(sumsPath()), checksums + 1);
}

void TestYayc::e2e_resume_data()
{
    QTest::addColumn<QByteArray>("part");  // left by an earlier download
    QTest::addColumn<QByteArray>("etag");  // in its .part.meta
    QTest::addColumn<int>("status");       // the server's answer
    QTest::newRow("same upload: 206") << m_pkg.left(m_pkg.size() / 2) << QByteArray("\"v1\"") << 206;
    // An older upload: its bytes must not be mixed in
    QTest::newRow("changed: 200") << QByteArray(1000, 'x') << QByteArray("\"v0\"") << 200;
    // Stopped after the last byte, before the rename: no second download
    QTest::newRow("complete: 416") << m_pkg << QByteArray("\"v1\"") << 416;
}

void TestYayc::e2e_resume()
{
    REQUIRE_E2E();
    QFETCH(QByteArray, part);
    QFETCH(QByteArray, etag);
    QFETCH(int, status);
    QVERIFY(seedPackage(cached() + ".part", part));
    QVERIFY(UpdateFetcher::writePartMeta(cached() + ".part.meta", {etag, ""}));

    UpdateManager um(e2eConfig());
    um.startUpdate("1.1.0");
    QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
    const auto request = m_server->last(assetPath());
    QCOMPARE(request.range, "bytes=" + QByteArray::number(part.size()) + '-');
    QCOMPARE(request.ifRange, etag);
    QCOMPARE(request.status, status);
    QCOMPARE(m_server->count(assetPath()), 1);
    QCOMPARE(readFile(cached()), m_pkg);
    QVERIFY(!QFileInfo::exists(cached() + ".part"));
    QVERIFY(!QFileInfo::exists(cached() + ".part.meta"));
}

void TestYayc::e2e_interruptedDownload()
{
    REQUIRE_E2E();
    m_server->routes[assetPath()].cutAfter = m_pkg.size() / 2;
    UpdateManager um(e2eConfig());
    um.startUpdate("1.1.0");
    QTRY_COMPARE(um.state(), UpdateManager::Failed);
    const qint64 kept = QFileInfo(cached() + ".part").size();
    QVERIFY(kept > 0 && kept < m_pkg.size());
    QVERIFY(um.canRetry());

    m_server->routes[assetPath()].cutAfter = -1;
    um.retry();
    QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
    QCOMPARE(m_server->last(assetPath()).range, "bytes=" + QByteArray::number(kept) + '-');
    QCOMPARE(readFile(cached()), m_pkg);
}

void TestYayc::e2e_stallTimeout()
{
    REQUIRE_E2E();
    m_server->routes[assetPath()].hang = true;
    UpdateManager um(e2eConfig());
    um.startUpdate("1.1.0");
    QTRY_COMPARE_WITH_TIMEOUT(um.state(), UpdateManager::Failed, 5000);
    QVERIFY(QFileInfo::exists(cached() + ".part")); // kept for a resume
}

void TestYayc::e2e_downloadMismatch()
{
    REQUIRE_E2E();
    setSums(QByteArray(64, '0'));
    UpdateManager um(e2eConfig());
    um.startUpdate("1.1.0");
    QTRY_COMPARE(um.state(), UpdateManager::Failed);
    QVERIFY(um.lastError().contains("checksum mismatch"));
    // A completed download that mismatches: the one case that deletes
    QVERIFY(!QFileInfo::exists(cached() + ".part"));
    QVERIFY(!QFileInfo::exists(cached()));
}

void TestYayc::e2e_verifyMismatchRedownload()
{
    REQUIRE_E2E();
    QVERIFY(downloadInFirstSession());
    setSums(QByteArray(64, '0')); // the release changed since
    const int downloads = m_server->count(assetPath());

    UpdateManager second(e2eConfig());
    second.resumeDownloaded();
    second.applyUpdate();
    QTRY_COMPARE(second.state(), UpdateManager::Failed);
    QCOMPARE(m_server->count(assetPath()), downloads + 1); // one fresh download, no loop
    QVERIFY(!QFileInfo::exists(cached()));
    QCOMPARE(readFile(installRoot() + "/yayc"), QByteArray("old"));
}

void TestYayc::e2e_notVerified()
{
    REQUIRE_E2E();
    QVERIFY(downloadInFirstSession());
    m_server->routes[sumsPath()].status = 503;

    UpdateManager second(e2eConfig());
    second.resumeDownloaded();
    second.applyUpdate();
    QTRY_COMPARE(second.state(), UpdateManager::NotVerified);
    QVERIFY(!second.lastError().isEmpty());
    QVERIFY(QFileInfo::exists(cached())); // a network error never deletes
    QVERIFY(!second.canRetry()); // "Retry check", not "Retry"

    // Retry here is the check again
    const int checksums = m_server->count(sumsPath());
    second.retry();
    QTRY_COMPARE(m_server->count(sumsPath()), checksums + 1);
    QTRY_COMPARE(second.state(), UpdateManager::NotVerified);

    second.applyUnverified();
    QTRY_COMPARE(second.state(), UpdateManager::ExitPending);
}

void TestYayc::e2e_notVerifiedAutoRetry()
{
    REQUIRE_E2E();
    // A click found no network: not an error of the package. The toggle goes on
    QVERIFY(downloadInFirstSession());
    m_server->routes[sumsPath()].status = 503;
    UpdateManager um(e2eConfig());
    um.resumeDownloaded();
    um.applyUpdate();
    QTRY_COMPARE(um.state(), UpdateManager::NotVerified);
    // Toggle on, still offline: silent
    um.setAutoUpdate(true);
    QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
    QVERIFY(um.lastError().isEmpty());
    // The network is back (networkFound): staged for the quit, no restart
    m_server->routes[sumsPath()].status = 200;
    QSignalSpy restart(&um, &UpdateManager::restartRequested);
    um.startAutoInstall();
    QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
    QCOMPARE(restart.count(), 0);
}

void TestYayc::e2e_assetMissingRetried()
{
    REQUIRE_E2E();
    // SHA256SUMS lists the package, the asset gives 404: the server's
    // fault. An error with Retry, not "no package" for the session
    m_server->routes[assetPath()].status = 404;
    UpdateManager um(e2eConfig());
    um.startUpdate("1.1.0");
    QTRY_COMPARE(um.state(), UpdateManager::Failed);
    QVERIFY(um.unavailableVersion().isEmpty());
    QVERIFY(!um.lastError().isEmpty());
    QVERIFY(um.canRetry());
    m_server->routes[assetPath()].status = 200;
    um.retry();
    QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
}

void TestYayc::e2e_retryKeepsAutoIntent()
{
    REQUIRE_E2E();
    QVERIFY(downloadInFirstSession());
    m_server->routes[sumsPath()].status = 403;
    UpdateManager um(e2eConfig());
    um.setAutoUpdate(true);
    um.resumeDownloaded(); // the toggle's verify: refused
    QTRY_VERIFY(um.state() == UpdateManager::ReadyToInstall && um.canRetry());
    // Retry of the toggle's work stages for the quit: no restart now
    m_server->routes[sumsPath()].status = 200;
    QSignalSpy restart(&um, &UpdateManager::restartRequested);
    um.retry();
    QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
    QCOMPARE(restart.count(), 0);
    QVERIFY(um.autoStaged());
}

void TestYayc::e2e_cancelDeclinesAuto()
{
    REQUIRE_E2E();
    m_server->routes[assetPath()].hang = true;
    {
        UpdateManager um(e2eConfig());
        um.setAutoUpdate(true);
        um.checkForUpdates();
        QTRY_COMPARE(um.state(), UpdateManager::Downloading);
        // Toggle off: the toggle's download stops (not declined)
        um.setAutoUpdate(false);
        QCOMPARE(um.state(), UpdateManager::Idle);
        um.setAutoUpdate(true);
        um.checkForUpdates();
        QTRY_COMPARE(um.state(), UpdateManager::Downloading);
        // The user's Cancel: not again this session by itself
        um.cancelDownload();
        QCOMPARE(um.state(), UpdateManager::Idle);
        const int assets = m_server->count(assetPath());
        um.checkForUpdates();
        QTRY_VERIFY(!um.checking());
        QTest::qWait(100);
        QCOMPARE(um.state(), UpdateManager::Idle);
        QCOMPARE(m_server->count(assetPath()), assets);
        // A click still downloads it
        m_server->routes[assetPath()].hang = false;
        um.startUpdate("1.1.0");
        QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
    }
    // Cancel of the toggle's verify: no stage this session
    m_server->routes[sumsPath()].hang = true;
    UpdateManager um(e2eConfig());
    um.setAutoUpdate(true);
    um.resumeDownloaded();
    QTRY_COMPARE(um.state(), UpdateManager::Verifying);
    um.cancelDownload();
    QCOMPARE(um.state(), UpdateManager::ReadyToInstall);
    m_server->routes[sumsPath()].hang = false;
    um.startAutoInstall(); // networkFound
    QTest::qWait(100);
    QCOMPARE(um.state(), UpdateManager::ReadyToInstall);
}

void TestYayc::e2e_withdrawStage()
{
    REQUIRE_E2E();
    {
        // An automatic stage: withdrawn, and not staged again this session
        UpdateManager um(e2eConfig());
        um.setAutoUpdate(true);
        um.startUpdate("1.1.0");
        QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
        QVERIFY(um.autoStaged());
        um.withdrawStage();
        QCOMPARE(um.state(), UpdateManager::ReadyToInstall);
        QVERIFY(!QFileInfo::exists(stagingDir()));
        um.startAutoInstall();
        QTest::qWait(100);
        QCOMPARE(um.state(), UpdateManager::ReadyToInstall);
    }
    // A manual stage of the installer waits for the quit: the user withdraws it
    servePackage("yayc-v1.1.0-win-x64.exe", m_pkg);
    UpdateConfig c = e2eConfig();
    c.installType = UpdateConfig::WindowsInstalled;
    c.updateMethod = UpdateConfig::Installer;
    UpdateManager um(c);
    QVERIFY(stageInSession(um));
    QVERIFY(!um.autoStaged());
    um.withdrawStage();
    QCOMPARE(um.state(), UpdateManager::ReadyToInstall);
    QCOMPARE(um.takeExitPlan().method, UpdateConfig::NoMethod);
    um.applyUpdate(); // offered again
    QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
}

void TestYayc::e2e_cancelPasswordPrompt()
{
    REQUIRE_E2E();
#ifndef Q_OS_LINUX
    QSKIP("pkexec is Linux only");
#else
    // A pkexec that waits, as at its password prompt
    servePackage("yayc_1.1.0_amd64.deb", m_pkg);
    const QString bin = m_tmp->filePath("bin");
    QVERIFY(QDir().mkpath(bin));
    QVERIFY(writeFile(bin + "/pkexec", "#!/bin/sh\nexec sleep 30\n", true));
    UpdateConfig c = e2eConfig();
    c.installType = UpdateConfig::LinuxDeb;
    c.updateMethod = UpdateConfig::ElevatedDpkg;
    c.pkexecPath = bin + "/pkexec";
    UpdateManager um(c);
    QVERIFY(!um.canCancel());
    um.startUpdate("1.1.0");
    QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
    um.applyUpdate();
    QTRY_VERIFY(um.state() == UpdateManager::Applying && um.canCancel());
    // Cancel: no error, the package on offer again
    um.cancelDownload();
    QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
    QVERIFY(um.lastError().isEmpty());
    QVERIFY(!um.canCancel());
#endif
}

void TestYayc::e2e_autoOfflineThenOnline()
{
    REQUIRE_E2E();
    QVERIFY(downloadInFirstSession());
    m_server->routes[sumsPath()].status = 503;

    UpdateManager um(e2eConfig());
    um.setAutoUpdate(true);
    QSignalSpy changed(&um, &UpdateManager::stateChanged);
    QSignalSpy restart(&um, &UpdateManager::restartRequested);
    um.resumeDownloaded(); // Ready → Verifying → (503) Ready
    QTRY_VERIFY(changed.count() >= 3);
    QCOMPARE(um.state(), UpdateManager::ReadyToInstall);
    QVERIFY(um.lastError().isEmpty()); // silent: no network yet is no error

    m_server->routes[sumsPath()].status = 200;
    um.startAutoInstall(); // what onNetworkFound does
    QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
    QCOMPARE(restart.count(), 0); // automatic: waits for the quit
}

void TestYayc::e2e_toggleOffDuringStage()
{
    REQUIRE_E2E();
    QVERIFY(downloadInFirstSession());
    UpdateManager um(e2eConfig());
    um.setAutoUpdate(true);
    bool sawApplying = false;
    connect(&um, &UpdateManager::stateChanged, &um, [&]() {
        if (um.state() == UpdateManager::Applying) {
            sawApplying = true;
            um.setAutoUpdate(false);
        }
    });
    um.resumeDownloaded();
    QTRY_VERIFY(sawApplying && um.state() == UpdateManager::ReadyToInstall);
    QVERIFY(!QFileInfo::exists(stagingDir()));
    QVERIFY(!um.finishOnExit());
    QCOMPARE(readFile(installRoot() + "/yayc"), QByteArray("old"));
}

void TestYayc::e2e_retryAfterRefusedVerify()
{
    REQUIRE_E2E();
    QVERIFY(downloadInFirstSession());
    UpdateManager um(e2eConfig());
    um.resumeDownloaded();
    // A newer release whose download fails: Retry would download it
    addRelease("1.2.0");
    m_server->routes[QByteArray("/releases/download/yayc-v1.2.0/yayc-1.2.0-linux-x86_64.tar.xz")]
            .status = 503;
    um.startUpdate("1.2.0");
    QTRY_VERIFY(um.state() == UpdateManager::ReadyToInstall && um.retryDownloads());

    // Then the ready package is refused: Retry now means checking it again.
    // A site rollback: the feed is about 1.1.0 again
    setFeed(feed("1.1.0", pkgSha256()));
    m_server->routes[sumsPath()].status = 403;
    um.applyUpdate();
    QTRY_VERIFY(um.state() == UpdateManager::ReadyToInstall && !um.lastError().isEmpty());
    QVERIFY(um.canRetry());
    QVERIFY(!um.retryDownloads());
    const int checksums = m_server->count(sumsPath());
    um.retry();
    QTRY_COMPARE(m_server->count(sumsPath()), checksums + 1);
    QCOMPARE(um.downloadVersion(), QString());
}

void TestYayc::e2e_freshWindowDoesNotSlide()
{
    REQUIRE_E2E();
    UpdateConfig c = e2eConfig();
    c.freshVerifyMs = 1500;
    UpdateManager um(c);
    QElapsedTimer sinceDownload;
    um.startUpdate("1.1.0");
    QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
    sinceDownload.start(); // the download fetched feed and checksums
    const int checksums = m_server->count(sumsPath());

    // Late in the window: the shortcut, no fetch
    QTest::qWait(int(qMax<qint64>(0, 900 - sinceDownload.elapsed())));
    um.setAutoUpdate(true);
    QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
    um.setAutoUpdate(false); // unstage
    QCOMPARE(um.state(), UpdateManager::ReadyToInstall);
    QCOMPARE(m_server->count(sumsPath()), checksums);

    // Past the window of the download, but within one restarted by the shortcut
    QTest::qWait(int(qMax<qint64>(0, 1600 - sinceDownload.elapsed())));
    um.setAutoUpdate(true);
    QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
    QCOMPARE(m_server->count(sumsPath()), checksums + 1);
}

void TestYayc::e2e_cancelReverifyKeepsError()
{
    REQUIRE_E2E();
    {
        UpdateManager um(e2eConfig());
        QVERIFY(stageInSession(um));
        QVERIFY(QDir(stagingDir()).removeRecursively()); // the commit cannot work now
        um.finishOnExit();
    }
    UpdateManager next(e2eConfig());
    next.resumeDownloaded();
    const QString error = next.lastError();
    QVERIFY(!error.isEmpty());

    m_server->routes[sumsPath()].hang = true;
    next.retry();
    QTRY_COMPARE(next.state(), UpdateManager::Verifying);
    next.cancelDownload();
    QCOMPARE(next.state(), UpdateManager::ReadyToInstall);
    QCOMPARE(next.lastError(), error);
    QVERIFY(next.canRetry());

    // The failed package never installs by itself
    QSignalSpy changed(&next, &UpdateManager::stateChanged);
    next.setAutoUpdate(true);
    next.startAutoInstall();
    QTest::qWait(200);
    QCOMPARE(changed.count(), 0);
}

void TestYayc::e2e_twoInstallsSameParent()
{
    REQUIRE_E2E();
    // Install A stages; install B, in the same parent dir, starts its cleanup
    UpdateManager a(e2eConfig());
    QVERIFY(stageInSession(a));
    QVERIFY(QFileInfo::exists(stagingDir()));

    const QString otherRoot = m_tmp->filePath("apps/yayc-test");
    QVERIFY(QDir().mkpath(otherRoot));
    QVERIFY(writeFile(otherRoot + "/yayc", "other", true));
    UpdateConfig c = e2eConfig();
    c.installRoot = otherRoot;
    c.exePath = otherRoot + "/yayc";
    c.updatesDir = UpdateConfig::updatesDirFor(m_tmp->filePath("cache"), otherRoot);
    c.cleanupDelayMs = 0;
    UpdateManager b(c);
    QTest::qWait(200);

    QVERIFY(QFileInfo::exists(stagingDir())); // A's staging survives
    QVERIFY(!a.finishOnExit());
    QCOMPARE(readFile(installRoot() + "/yayc"), QByteArray("new"));
    QCOMPARE(readFile(otherRoot + "/yayc"), QByteArray("other"));
}

void TestYayc::e2e_helperScriptAtExit()
{
    REQUIRE_E2E();
    // Windows installer method, driven on any OS: only the helper start differs
    const QByteArray exe = "yayc-v1.1.0-win-x64.exe";
    servePackage(exe, m_pkg);
    UpdateConfig c = e2eConfig();
    c.installType = UpdateConfig::WindowsInstalled;
    c.updateMethod = UpdateConfig::Installer;
    c.relaunchArgs = {"-c", "C:/cfg/my.ini"}; // as main.cpp passes them: -c absolute
    UpdateManager um(c);
    QVERIFY(stageInSession(um));

    // Staged, but no script yet: a session that never exits leaves none behind
    const QString script = c.updatesDir + "/yayc-update.cmd";
    const QString pidFile = c.updatesDir + "/yayc-update.pid";
    QVERIFY(!QFileInfo::exists(script));
    UpdateInstall::ExitPlan plan = um.takeExitPlan();
    QVERIFY(plan.helperScript.contains(QString(exe)));
    QVERIFY(plan.helperScript.contains("/SILENT"));
    QVERIFY(plan.helperScript.contains("yayc\" \"-c\" \"C:/cfg/my.ini\"\r\n"));
    // The hash the download was checked against reaches the admin step
    QVERIFY(plan.helperScript.contains(QString::fromLatin1(
            QCryptographicHash::hash(m_pkg, QCryptographicHash::Sha256).toHex())));
#ifdef Q_OS_WIN
    QSKIP("would start the real helper");
#else
    // The helper cannot start here: the failure path of a real one
    QVERIFY(writeFile(pidFile, "1 stale", false));
    QVERIFY(!UpdateInstall::runExitPlan(plan, true)); // main.cpp relaunches itself
    QVERIFY(!QFileInfo::exists(script));
    QVERIFY(!QFileInfo::exists(pidFile));
    QVERIFY(readFile(installErrorFile()).contains("Windows only"));
#endif
}

void TestYayc::e2e_resumeStage()
{
    REQUIRE_E2E();
    const QString marker = m_tmp->filePath("cache/updates/staged-version");
    {
        // A complete stage leaves a marker; an undone one takes it along
        UpdateManager um(e2eConfig());
        um.setAutoUpdate(true);
        um.startUpdate("1.1.0");
        QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
        QCOMPARE(readFile(marker), QByteArray("1.1.0"));
        um.setAutoUpdate(false);
        QVERIFY(!QFileInfo::exists(marker));
        // Staged again, then the session ends at logoff: no exit plan runs
        um.setAutoUpdate(true);
        QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
        QVERIFY(QFileInfo::exists(marker));
    }
    UpdateConfig c = e2eConfig();
    c.updateMethod = UpdateConfig::HelperSwap;
    {
        // Not newer than this build, or not Windows portable: left alone
        UpdateConfig same = c;
        same.currentVersion = "1.1.0";
        QVERIFY(!UpdateManager(same).resumeStage());
        QVERIFY(!UpdateManager(e2eConfig()).resumeStage());
#ifdef Q_OS_LINUX
        // The helper of an earlier exit still works on it
        QVERIFY(writeLiveHelper());
        QVERIFY(!UpdateManager(c).resumeStage());
        QVERIFY(QFile::remove(helperScript()));
#endif
    }
    // The next start installs it before the UI: an exit plan with a relaunch
    UpdateManager next(c);
    QVERIFY(next.resumeStage());
    QCOMPARE(next.state(), UpdateManager::ExitPending);
    QCOMPARE(next.readyVersion(), QStringLiteral("1.1.0"));
    UpdateInstall::ExitPlan plan = next.takeExitPlan();
    QCOMPARE(plan.method, UpdateConfig::HelperSwap);
    QCOMPARE(plan.stagedRoot, stagingDir() + "/yayc-1.1.0");
    QVERIFY(plan.helperScript.contains("start \"\" /D"));
#ifndef Q_OS_WIN
    // The helper cannot start here: a failed install, recorded. No retry at
    // every start then: the error shows
    QVERIFY(!UpdateInstall::runExitPlan(plan, true));
    plan.lock.reset(); // the process ends: the owner lock goes
    UpdateManager after(c);
    QVERIFY(!after.resumeStage());
    after.resumeDownloaded();
    QVERIFY(after.lastError().contains("Windows only"));
#endif
}

void TestYayc::helperSwapNeedsStagedExe()
{
    // The staged tree lost the exe after the stage (antivirus): no helper
    UpdateInstall::ExitPlan plan;
    plan.method = UpdateConfig::HelperSwap;
    plan.version = "1.1.0";
    plan.installRoot = installRoot();
    plan.stagedRoot = stagingDir() + "/yayc-1.1.0";
    plan.exeName = "yayc";
    plan.updatesDir = m_tmp->filePath("cache/updates");
    plan.shippedFiles = {QStringLiteral("yayc")};
    plan.helperScript = "rem";
    QVERIFY(QDir().mkpath(plan.stagedRoot));
    QVERIFY(QDir().mkpath(plan.updatesDir));
    QVERIFY(!UpdateInstall::runExitPlan(plan, true));
    QVERIFY(readFile(installErrorFile()).contains("staged tree does not contain yayc"));
    QVERIFY(!QFileInfo::exists(helperScript()));
#ifndef Q_OS_WIN
    // With the exe it goes on to the helper (which cannot start here)
    QVERIFY(writeFile(plan.stagedRoot + "/yayc", "new", true));
    QVERIFY(!UpdateInstall::runExitPlan(plan, true));
    QVERIFY(readFile(installErrorFile()).contains("Windows only"));
#endif
}

void TestYayc::processStartToken()
{
#ifdef Q_OS_LINUX
    const QString self = UpdateInstall::processStartToken(QCoreApplication::applicationPid());
    QVERIFY(!self.isEmpty());
    QCOMPARE(UpdateInstall::processStartToken(QCoreApplication::applicationPid()), self);
    QProcess child;
    child.start("sleep", {"30"});
    QVERIFY(child.waitForStarted());
    const qint64 pid = child.processId();
    const QString token = UpdateInstall::processStartToken(pid);
    QVERIFY(!token.isEmpty());
    QVERIFY(token != self);
    child.kill();
    QVERIFY(child.waitForFinished());
    QVERIFY(UpdateInstall::processStartToken(pid).isEmpty()); // gone
#else
    QSKIP("checked on Windows with the helper");
#endif
}

void TestYayc::e2e_helperPendingBlocksStage()
{
    REQUIRE_E2E();
#ifndef Q_OS_LINUX
    QSKIP("needs processStartToken()");
#endif
    // Quit, then a quick relaunch: the helper of that exit still works
    QVERIFY(downloadInFirstSession());
    QVERIFY(writeLiveHelper());
    {
        UpdateManager um(e2eConfig());
        um.resumeDownloaded();
        um.applyUpdate();
        QTRY_VERIFY(um.state() == UpdateManager::ReadyToInstall && !um.lastError().isEmpty());
        QVERIFY(um.lastError().contains("still finishing"));
        QVERIFY(!QFileInfo::exists(stagingDir()));
        QVERIFY(QFileInfo::exists(helperScript())); // untouched
    }
    {
        // Auto: no error, the lock is released, a later try stages
        UpdateManager um(e2eConfig());
        um.resumeDownloaded();
        const int checksums = m_server->count(sumsPath());
        um.setAutoUpdate(true);
        QTRY_VERIFY(m_server->count(sumsPath()) > checksums
                    && um.state() == UpdateManager::ReadyToInstall);
        QVERIFY(um.lastError().isEmpty());

        QVERIFY(QFile::remove(helperScript())); // the helper is done
        um.startAutoInstall();
        QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
    }
}

void TestYayc::e2e_orphanHelperIgnored()
{
    REQUIRE_E2E();
#ifndef Q_OS_LINUX
    QSKIP("needs processStartToken()");
#endif
    QVERIFY(downloadInFirstSession());
    // A dead helper, and a live process that is not the helper (pid reused)
    const QList<std::pair<qint64, QByteArray>> orphans{
        {deadPid(), "1"}, {QCoreApplication::applicationPid(), "not-its-start-time"}};
    for (const auto &[pid, token] : orphans) {
        QVERIFY(writeHelper(pid, token));
        UpdateManager um(e2eConfig());
        um.resumeDownloaded();
        um.applyUpdate();
        QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
        QVERIFY(!QFileInfo::exists(helperScript()));
        QVERIFY(!QFileInfo::exists(helperPid()));
        um.setAutoUpdate(false);
        QVERIFY(QDir(stagingDir()).removeRecursively() || !QFileInfo::exists(stagingDir()));
    }
}

void TestYayc::e2e_extractFailures()
{
    REQUIRE_E2E();
    // A verified package that is no archive: the extractor fails
    const QByteArray junk(1000, 'j');
    servePackage(asset, junk);
    {
        UpdateManager um(e2eConfig());
        um.startUpdate("1.1.0");
        QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
        um.applyUpdate();
        QTRY_VERIFY(!um.lastError().isEmpty());
        QCOMPARE(um.state(), UpdateManager::ReadyToInstall);
        QVERIFY(um.lastError().startsWith("extraction failed"));
        QVERIFY(!QFileInfo::exists(stagingDir()));
    }
#ifdef Q_OS_WIN
    QSKIP("the Windows extractor exists here");
#endif
    // The Windows extractor on another OS: it never starts
    UpdateConfig c = e2eConfig();
    c.updateMethod = UpdateConfig::HelperSwap;
    UpdateManager um(c);
    um.resumeDownloaded();
    um.applyUpdate();
    QTRY_VERIFY(!um.lastError().isEmpty());
    QCOMPARE(um.state(), UpdateManager::ReadyToInstall);
    // By path: \System32\tar.exe ($SystemRoot is empty here)
    QVERIFY(um.lastError().startsWith("cannot run "));
    QVERIFY(um.lastError().contains("\\System32\\tar.exe"));
    QVERIFY(!QFileInfo::exists(stagingDir()));
    QVERIFY(!QFileInfo::exists(m_tmp->filePath("cache/updates/staged-version")));
}

void TestYayc::e2e_manualDeb()
{
    REQUIRE_E2E();
    // A deb without pkexec: the user runs the shown command
    const QByteArray deb = "yayc_1.1.0_amd64.deb";
    servePackage(deb, m_pkg);
    UpdateConfig c = e2eConfig();
    c.installType = UpdateConfig::LinuxDeb;
    c.updateMethod = UpdateConfig::ManualDeb;
    const QString sha = QString::fromLatin1(
            QCryptographicHash::hash(m_pkg, QCryptographicHash::Sha256).toHex());
    {
        UpdateManager um(c);
        um.startUpdate("1.1.0");
        QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
        QVERIFY(um.manualInstallCommand().isEmpty());
        um.applyUpdate();
        QTRY_COMPARE(um.state(), UpdateManager::ManualInstall);
        // The root script of pkexec under sudo, with the hash just checked
        QCOMPARE(um.manualInstallCommand(),
                 UpdateLinux::manualDebCommand(um.updateFilePath("1.1.0"), sha, "yayc"));
        QVERIFY(um.manualInstallCommand().startsWith("/usr/bin/sudo '/bin/sh' '-c' "));
        QVERIFY(um.manualInstallCommand().endsWith(" && pkill -x 'yayc'"));
    }
    // Offline in a later session: only "Retry check". Root would check the
    // file against its own hash: a same-user process may have planted it
    m_server->routes[sumsPath()].status = 503;
    UpdateManager um(c);
    um.resumeDownloaded();
    um.applyUpdate();
    QTRY_COMPARE(um.state(), UpdateManager::NotVerified);
    QVERIFY(!um.canInstallUnverified());
    um.applyUnverified();
    QCOMPARE(um.state(), UpdateManager::NotVerified);
    QVERIFY(um.manualInstallCommand().isEmpty());
}

void TestYayc::e2e_pkexecWithoutAgent()
{
    REQUIRE_E2E();
#ifndef Q_OS_LINUX
    QSKIP("pkexec is Linux only");
#else
    servePackage("yayc_1.1.0_amd64.deb", m_pkg);
    // A pkexec that exits with the code in a file
    const QString bin = m_tmp->filePath("bin");
    const QString code = m_tmp->filePath("pkexec-exit");
    QVERIFY(QDir().mkpath(bin));
    QVERIFY(writeFile(bin + "/pkexec", "#!/bin/sh\nexit $(cat '" + code.toUtf8() + "')\n", true));
    UpdateConfig c = e2eConfig();
    c.installType = UpdateConfig::LinuxDeb;
    c.updateMethod = UpdateConfig::ElevatedDpkg;
    c.pkexecPath = bin + "/pkexec"; // by path, never from PATH
    UpdateManager um(c);
    um.startUpdate("1.1.0");
    QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);

    // 126, the prompt dismissed: a plain failure, Retry
    QVERIFY(writeFile(code, "126", false));
    um.applyUpdate();
    QTRY_VERIFY(um.state() == UpdateManager::ReadyToInstall && !um.lastError().isEmpty());
    QVERIFY(um.canRetry());
    QCOMPARE(um.updateMethod(), UpdateConfig::ElevatedDpkg);

    // Then offline: the retry checks again (no fresh shortcut). The hash
    // checked at the download still allows Install anyway
    m_server->routes[sumsPath()].status = 503;
    const int sums = m_server->count(sumsPath());
    um.retry();
    QTRY_COMPARE(um.state(), UpdateManager::NotVerified);
    QCOMPARE(m_server->count(sumsPath()), sums + 1);
    QVERIFY(um.canInstallUnverified());
    um.applyUnverified();
    QTRY_VERIFY(um.state() == UpdateManager::ReadyToInstall && !um.lastError().isEmpty());
    m_server->routes[sumsPath()].status = 200;

    // 127, no polkit agent: the command the user runs, from now on
    QVERIFY(writeFile(code, "127", false));
    um.retry();
    QTRY_COMPARE(um.state(), UpdateManager::ManualInstall);
    QCOMPARE(um.updateMethod(), UpdateConfig::ManualDeb);
    QCOMPARE(um.manualInstallCommand(),
             UpdateLinux::manualDebCommand(um.updateFilePath("1.1.0"),
                                             QString::fromLatin1(pkgSha256()), "yayc"));
#endif
}

void TestYayc::e2e_commitErrorPersisted()
{
    REQUIRE_E2E();
    {
        UpdateManager um(e2eConfig());
        QVERIFY(stageInSession(um));
        QVERIFY(QDir(stagingDir()).removeRecursively()); // the commit cannot work now
        um.finishOnExit();
        QCOMPARE(readFile(installRoot() + "/yayc"), QByteArray("old"));
    }
    QVERIFY(QFileInfo::exists(installErrorFile()));

    UpdateManager next(e2eConfig());
    next.setAutoUpdate(true);
    QSignalSpy changed(&next, &UpdateManager::stateChanged);
    next.resumeDownloaded();
    QCOMPARE(next.state(), UpdateManager::ReadyToInstall);
    QVERIFY(next.lastError().contains("does not contain")); // the reason; QML adds the prefix
    QVERIFY(next.canRetry());
    QTest::qWait(200);
    QCOMPARE(changed.count(), 1); // shown, not retried by itself
}

void TestYayc::e2e_secondInstancePassive()
{
    REQUIRE_E2E();
    // Two instances of one install (-c a.ini, -c b.ini): one does the update work
    auto first = std::make_unique<UpdateManager>(e2eConfig());
    QVERIFY(stageInSession(*first));
    QVERIFY(!first->updatesElsewhere());

    UpdateManager second(e2eConfig());
    QSignalSpy changed(&second, &UpdateManager::stateChanged);
    second.resumeDownloaded();
    QVERIFY(second.updatesElsewhere()); // the UI says so, no error
    QCOMPARE(second.state(), UpdateManager::Idle);
    QVERIFY(second.lastError().isEmpty());
    QCOMPARE(changed.count(), 1);
    second.applyUpdate();
    second.startUpdate("1.1.0");
    QCOMPARE(second.state(), UpdateManager::Idle);

    // The first one quits: the next automatic step takes over, and finds the package
    first.reset();
    second.startAutoInstall(); // as networkFound
    QVERIFY(!second.updatesElsewhere());
    QTRY_COMPARE(second.state(), UpdateManager::ReadyToInstall);
    QCOMPARE(second.readyVersion(), QStringLiteral("1.1.0"));
}

void TestYayc::e2e_downloadSecondInstance()
{
    REQUIRE_E2E();
    // Two instances of one install share one .part: only the owner downloads
    m_server->routes[assetPath()].hang = true;
    auto first = std::make_unique<UpdateManager>(e2eConfig());
    first->startUpdate("1.1.0");
    QTRY_COMPARE(m_server->count(assetPath()), 1);

    // The second backs off: no request, no state, no error
    const int feeds = m_server->count(feedPath());
    UpdateManager second(e2eConfig());
    second.startUpdate("1.1.0");
    QCOMPARE(second.state(), UpdateManager::Idle);
    QVERIFY(second.lastError().isEmpty());
    QVERIFY(second.updatesElsewhere());
    QTest::qWait(100);
    QCOMPARE(m_server->count(feedPath()), feeds);

    // Once the first one is gone, a later check downloads
    first.reset();
    m_server->routes[assetPath()].hang = false;
    second.startUpdate("1.1.0");
    QTRY_COMPARE(second.state(), UpdateManager::ReadyToInstall);
    QCOMPARE(readFile(cached()), m_pkg);
}

void TestYayc::othersRunning()
{
    const QString updates = m_tmp->filePath("cache/updates");
    QVERIFY(!UpdateInstall::othersRunning(updates)); // no dir yet
    const auto own = UpdateInstall::registerRunning(updates);
    QVERIFY(own);
    QVERIFY(!UpdateInstall::othersRunning(updates)); // this process only
    // Another live one (held here, under another name)
    const QString other = updates + "/running/999999999.lock";
    {
        QLockFile live(other);
        QVERIFY(live.tryLock(0));
        QVERIFY(UpdateInstall::othersRunning(updates));
    }
    QVERIFY(!UpdateInstall::othersRunning(updates));
#ifdef Q_OS_LINUX
    // A dead one's lock, and an empty file (a probe of the helper): not running, removed
    QVERIFY(writeFile(other, QByteArray::number(deadPid()) + "\nyayc\n" + QSysInfo::machineHostName().toUtf8() + "\n", false));
    QVERIFY(writeFile(updates + "/running/1.lock", "", false));
    // Fresh: maybe a QLockFile not written yet. Old: gone
    QVERIFY(UpdateInstall::othersRunning(updates));
    QFile empty(updates + "/running/1.lock");
    QVERIFY(empty.open(QIODevice::ReadWrite));
    QVERIFY(empty.setFileTime(QDateTime::currentDateTime().addSecs(-120), QFileDevice::FileModificationTime));
    empty.close();
    QVERIFY(!UpdateInstall::othersRunning(updates));
    QVERIFY(!QFileInfo::exists(other));
    QVERIFY(!QFileInfo::exists(updates + "/running/1.lock"));
#endif
}

void TestYayc::e2e_commitWaitsForOthers()
{
    REQUIRE_E2E();
    const QString updates = m_tmp->filePath("cache/updates");
    QLockFile other(updates + "/running/999999999.lock");
    {
        UpdateManager um(e2eConfig());
        QVERIFY(stageInSession(um));
        QVERIFY(QDir().mkpath(updates + "/running"));
        QVERIFY(other.tryLock(0)); // another YAYC of this install starts
        // It runs from these files: nothing installs at this exit, no error
        QVERIFY(!um.finishOnExit());
    }
    QCOMPARE(readFile(installRoot() + "/yayc"), QByteArray("old"));
    QVERIFY(QFileInfo::exists(stagingDir()));
    QVERIFY(!QFileInfo::exists(installErrorFile()));

    // A click on Apply says so at once (a restart would not install)
    {
        UpdateManager um(e2eConfig());
        um.resumeDownloaded();
        um.applyUpdate();
        QTRY_VERIFY(um.state() == UpdateManager::ReadyToInstall && !um.lastError().isEmpty());
        QVERIFY(um.lastError().contains("close the other YAYC windows"));
    }
    // The other one quits: the next exit installs
    other.unlock();
    UpdateManager um(e2eConfig());
    QVERIFY(stageInSession(um));
    um.finishOnExit();
    QCOMPARE(readFile(installRoot() + "/yayc"), QByteArray("new"));
}

void TestYayc::deferToHelper()
{
#ifndef Q_OS_LINUX
    QSKIP("needs processStartToken()");
#else
    UpdateConfig c = e2eConfig();
    const QString request = c.updatesDir + "/relaunch-request";
    QVERIFY(!UpdateManager(c).deferToHelper()); // not a Windows method
    c.updateMethod = UpdateConfig::HelperSwap;
    QVERIFY(!UpdateManager(c).deferToHelper()); // no helper
    // The helper of an earlier exit works: quit, it starts YAYC once done
    QVERIFY(writeLiveHelper());
    QVERIFY(UpdateManager(c).deferToHelper());
    QVERIFY(QFileInfo::exists(request));
    // The helper died: an orphan, its request goes too; this start runs
    QVERIFY(writeHelper(deadPid(), "1"));
    QVERIFY(!UpdateManager(c).deferToHelper());
    QVERIFY(!QFileInfo::exists(request));
    QVERIFY(!QFileInfo::exists(helperScript()));
#endif
}

void TestYayc::singleInstance()
{
    const QString cfg = m_tmp->filePath("cfg/a.ini");
    auto first = std::make_unique<SingleInstance>();
    QVERIFY(first->claim(cfg));
    QVERIFY(QFileInfo::exists(cfg + ".instance.lock"));
    {
        // QSettings locks <file>.lock to write: never the same lock
        QSettings settings(cfg, QSettings::IniFormat);
        settings.setValue("k", 1);
        settings.sync();
        QCOMPARE(settings.status(), QSettings::NoError);
    }
    QSignalSpy shown(first.get(), &SingleInstance::activationRequested);
    // Same file: the first one is asked to show its window
    SingleInstance second;
    QVERIFY(!second.claim(cfg, 0));
    QTRY_COMPARE(shown.count(), 1);
    // Another file: its own
    SingleInstance other;
    QVERIFY(other.claim(m_tmp->filePath("cfg/b.ini")));
    QVERIFY(SingleInstance::serverName(cfg) != SingleInstance::serverName(m_tmp->filePath("cfg/b.ini")));
    // The first one quits (a restart): the next start owns it
    first.reset();
    SingleInstance next;
    QVERIFY(next.claim(cfg, 0));

    // Held, but nobody answers (quitting): waits for the lock, then gives up
    const QString cfg2 = m_tmp->filePath("cfg/c.ini");
    QLockFile held(cfg2 + ".instance.lock");
    QVERIFY(held.tryLock(0));
    QElapsedTimer t;
    t.start();
    SingleInstance late;
    QVERIFY(!late.claim(cfg2, 300));
    QVERIFY(t.elapsed() >= 250);

    // At the quit: no more activations; a start waits for the lock instead
    auto quitting = std::make_unique<SingleInstance>();
    const QString cfg3 = m_tmp->filePath("cfg/d.ini");
    QVERIFY(quitting->claim(cfg3));
    std::unique_ptr<QLockFile> kept = quitting->release();
    QVERIFY(kept && kept->isLocked());
    SingleInstance waiting;
    QVERIFY(!waiting.claim(cfg3, 200)); // nobody answers, the lock stays held
    kept.reset();
    SingleInstance after;
    QVERIFY(after.claim(cfg3, 0));
}

void TestYayc::e2e_cancelRedownloadDropsApply()
{
    REQUIRE_E2E();
    QVERIFY(downloadInFirstSession());
    setSums(QByteArray(64, '0'));
    m_server->routes[assetPath()].hang = true;

    UpdateManager um(e2eConfig());
    QSignalSpy restart(&um, &UpdateManager::restartRequested);
    um.resumeDownloaded();
    um.applyUpdate(); // mismatch: re-download, then this Apply would go on
    QTRY_COMPARE(um.state(), UpdateManager::Downloading);
    um.cancelDownload();
    QCOMPARE(um.state(), UpdateManager::Idle);

    // A later download only gets ready: the cancelled Apply is gone
    setSums(pkgSha256());
    m_server->routes[assetPath()].hang = false;
    um.startUpdate("1.1.0");
    QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
    QTest::qWait(200);
    QCOMPARE(um.state(), UpdateManager::ReadyToInstall);
    QCOMPARE(restart.count(), 0);
}

void TestYayc::e2e_verifyRefused()
{
    REQUIRE_E2E();
    QVERIFY(downloadInFirstSession());
    m_server->routes[sumsPath()].status = 403; // no outage: no "install anyway"

    UpdateManager um(e2eConfig());
    um.resumeDownloaded();
    um.applyUpdate();
    QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
    QVERIFY(!um.lastError().isEmpty());
    QVERIFY(um.canRetry());
    QVERIFY(QFileInfo::exists(cached()));
}

void TestYayc::e2e_newerReadyDropsOlder()
{
    REQUIRE_E2E();
    // A ready 1.0.5 from an earlier session
    UpdateManager um(e2eConfig());
    const QString older = um.updateFilePath("1.0.5");
    QVERIFY(seedPackage(older, "pkg"));
    um.resumeDownloaded();
    QCOMPARE(um.state(), UpdateManager::ReadyToInstall);

    um.startUpdate("1.1.0");
    QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
    QCOMPARE(um.readyVersion(), QStringLiteral("1.1.0"));
    QVERIFY(!QFileInfo::exists(QFileInfo(older).path()));
}

void TestYayc::e2e_partialRelease()
{
    REQUIRE_E2E();
    // No package for this install in the release: no error
    unlistPackage("1.1.0");
    UpdateManager um(e2eConfig());
    um.startUpdate("1.1.0");
    QTRY_COMPARE(um.state(), UpdateManager::Idle);
    QVERIFY(um.lastError().isEmpty());
    QCOMPARE(um.unavailableVersion(), QStringLiteral("1.1.0"));
    QVERIFY(!um.canRetry());
    QCOMPARE(m_server->count(assetPath()), 0);
}

void TestYayc::e2e_unpublishedRelease()
{
    REQUIRE_E2E();
    // APPVERSION already bumped, the release still a draft: retried, not skipped
    m_server->routes[sumsPath()].status = 404;
    UpdateManager um(e2eConfig());
    um.startUpdate("1.1.0");
    QTRY_COMPARE(um.state(), UpdateManager::Idle);
    QVERIFY(um.lastError().isEmpty());
    QVERIFY(um.unavailableVersion().isEmpty());

    m_server->routes[sumsPath()].status = 200;
    um.startUpdate("1.1.0");
    QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
}

void TestYayc::e2e_newerDownloadKeepsReadyError()
{
    REQUIRE_E2E();
    QVERIFY(downloadInFirstSession());
    // The ready 1.1.0 failed at the last exit
    QVERIFY(writeFile(installErrorFile(), R"({"version":"1.1.0","error":"disk full"})", false));

    UpdateManager um(e2eConfig());
    um.setAutoUpdate(true);
    um.resumeDownloaded();
    QVERIFY(!um.lastError().isEmpty());
    um.startUpdate("1.2.0"); // no release on the server
    QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
    QVERIFY(um.lastError().contains("disk full"));
    um.startAutoInstall(); // what onNetworkFound does: still nothing by itself
    QCOMPARE(um.state(), UpdateManager::ReadyToInstall);
}

void TestYayc::e2e_resumeNewestDownloaded()
{
    REQUIRE_E2E();
    QVERIFY(downloadInFirstSession()); // 1.1.0
    UpdateManager um(e2eConfig());
    const QString older = um.updateFilePath("1.0.5");
    QVERIFY(seedPackage(older, "pkg"));
    QVERIFY(QDir().mkpath(m_tmp->filePath("cache/updates/0.9.0"))); // not newer than 1.0.0

    um.resumeDownloaded();
    QCOMPARE(um.state(), UpdateManager::ReadyToInstall);
    QCOMPARE(um.readyVersion(), QStringLiteral("1.1.0"));
}

void TestYayc::e2e_unavailableNewerKeepsReadyClean()
{
    REQUIRE_E2E();
    QVERIFY(downloadInFirstSession()); // 1.1.0, no error
    UpdateManager um(e2eConfig());
    um.resumeDownloaded();
    addRelease("1.2.0");
    m_server->routes["/releases/download/yayc-v1.2.0/SHA256SUMS"].status = 503;

    um.startUpdate("1.2.0"); // a network error
    QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
    QVERIFY(!um.lastError().isEmpty());
    QVERIFY(um.retryDownloads());

    // Retried: 1.2.0 has no package for this install after all
    unlistPackage("1.2.0");
    um.retry();
    QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
    QCOMPARE(um.unavailableVersion(), QStringLiteral("1.2.0"));
    QVERIFY(um.lastError().isEmpty()); // the download error went with it
    QVERIFY(!um.canRetry());

    // 1.1.0 itself never failed: the toggle may stage it. A site rollback:
    // the feed is about 1.1.0 again
    setFeed(feed("1.1.0", pkgSha256()));
    um.setAutoUpdate(true);
    QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
}

void TestYayc::e2e_newerReplacesAutoStage()
{
    REQUIRE_E2E();
    QVERIFY(downloadInFirstSession()); // 1.1.0 left from a past session
    UpdateManager um(e2eConfig());
    um.setAutoUpdate(true);
    um.resumeDownloaded();
    QTRY_COMPARE(um.state(), UpdateManager::ExitPending); // staged before the version check

    addRelease("1.2.0");
    um.startUpdate("1.2.0"); // the check finds a newer release
    QTRY_VERIFY(um.state() == UpdateManager::ExitPending && um.readyVersion() == "1.2.0");
    QVERIFY(!QFileInfo::exists(QFileInfo(cached()).path())); // 1.1.0 gone
    QVERIFY(!um.finishOnExit());
    QCOMPARE(readFile(installRoot() + "/yayc"), QByteArray("new"));
}

void TestYayc::e2e_autoStageSurvivesNewerFailure()
{
    REQUIRE_E2E();
    QVERIFY(downloadInFirstSession()); // 1.1.0
    UpdateManager um(e2eConfig());
    um.setAutoUpdate(true);
    um.resumeDownloaded();
    QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
    const auto stillStaged = [&]() {
        return um.state() == UpdateManager::ExitPending && um.readyVersion() == "1.1.0"
                && QFileInfo::exists(stagingDir());
    };

    // The check finds 1.2.0; the feed is still about 1.1.0
    um.startUpdate("1.2.0");
    QCOMPARE(um.state(), UpdateManager::Downloading);
    QTRY_VERIFY(stillStaged());
    QVERIFY(um.lastError().isEmpty());

    // A network error: shown, with Retry, the stage stays
    addRelease("1.2.0");
    const QByteArray sums12 = "/releases/download/yayc-v1.2.0/SHA256SUMS";
    m_server->routes[sums12].status = 503;
    um.startUpdate("1.2.0");
    QTRY_VERIFY(stillStaged() && !um.lastError().isEmpty());
    QVERIFY(um.canRetry() && um.retryDownloads());
    QVERIFY(um.autoStaged());

    // Cancelled
    m_server->routes[sums12].status = 200;
    m_server->routes["/releases/download/yayc-v1.2.0/yayc-1.2.0-linux-x86_64.tar.xz"].hang = true;
    um.startUpdate("1.2.0");
    QCOMPARE(um.state(), UpdateManager::Downloading);
    um.cancelDownload();
    QVERIFY(stillStaged());

    // Toggle off: the stage goes, the download goes on
    um.startUpdate("1.2.0");
    um.setAutoUpdate(false);
    QCOMPARE(um.state(), UpdateManager::Downloading);
    QVERIFY(!QFileInfo::exists(stagingDir()));
    um.cancelDownload();
    QCOMPARE(um.state(), UpdateManager::ReadyToInstall);

    // A quit during the newer download installs the staged one
    um.setAutoUpdate(true);
    QTRY_VERIFY(stillStaged());
    um.startUpdate("1.2.0");
    QCOMPARE(um.state(), UpdateManager::Downloading);
    QVERIFY(!um.finishOnExit());
    QCOMPARE(readFile(installRoot() + "/yayc"), QByteArray("new"));
}

void TestYayc::e2e_clickFollowsFeed()
{
    REQUIRE_E2E();
    addRelease("1.2.0"); // the feed has moved on; the UI still offers 1.1.0
    {
        UpdateManager um(e2eConfig());
        QSignalSpy found(&um, &UpdateManager::latestVersionFound);
        um.startUpdate("1.1.0"); // a click
        QTRY_COMPARE(found.count(), 1);
        // Offered next: no extra request, no error
        QCOMPARE(found.at(0).at(0).toString(), QStringLiteral("1.2.0"));
        QCOMPARE(um.state(), UpdateManager::Idle);
        QVERIFY(um.lastError().isEmpty());
        QCOMPARE(m_server->count(feedPath()), 1);
    }
    // The automatic check (APPVERSION still 1.1.0): O2, the next check finds it
    UpdateManager um(e2eConfig());
    um.setAutoUpdate(true);
    QSignalSpy found(&um, &UpdateManager::latestVersionFound);
    um.checkForUpdates();
    QTRY_VERIFY(m_server->count(feedPath()) == 2 && um.state() == UpdateManager::Idle);
    QCOMPARE(found.count(), 1);
    QCOMPARE(found.at(0).at(0).toString(), QStringLiteral("1.1.0"));
}

void TestYayc::e2e_openDmgThenRetry()
{
    REQUIRE_E2E();
    servePackage("yayc-1.1.0-macos-universal.dmg", m_pkg);
    UpdateConfig c = e2eConfig();
    c.installType = UpdateConfig::MacOSBundle;
    c.updateMethod = UpdateConfig::OpenDmg;
    UrlCatcher opened;
    QDesktopServices::setUrlHandler("file", &opened, "open");
    const auto restore = qScopeGuard([]() { QDesktopServices::unsetUrlHandler("file"); });
    UpdateManager um(c);
    um.startUpdate("1.1.0");
    QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
    um.applyUpdate();
    QTRY_COMPARE(opened.urls.size(), 1);
    QCOMPARE(um.state(), UpdateManager::Idle);
    QVERIFY(um.readyVersion().isEmpty());

    // A later download of the same version fails: Retry gets through
    m_server->routes[sumsPath()].status = 503;
    um.startUpdate("1.1.0");
    QTRY_COMPARE(um.state(), UpdateManager::Failed);
    QVERIFY(um.canRetry());
    m_server->routes[sumsPath()].status = 200;
    um.retry();
    QTRY_COMPARE(um.state(), UpdateManager::ReadyToInstall);
    QCOMPARE(um.readyVersion(), QStringLiteral("1.1.0"));
}

void TestYayc::e2e_retryTakesLatest()
{
    REQUIRE_E2E();
    QVERIFY(downloadInFirstSession()); // 1.1.0
    UpdateManager um(e2eConfig());
    um.resumeDownloaded();
    addRelease("1.2.0");
    const QByteArray sums12 = "/releases/download/yayc-v1.2.0/SHA256SUMS";
    m_server->routes[sums12].status = 503;
    um.startUpdate("1.2.0");
    QTRY_VERIFY(um.state() == UpdateManager::ReadyToInstall && um.retryDownloads());

    // Meanwhile 1.3.0 is out; the check (toggle off) only reports it
    addRelease("1.3.0");
    m_server->routes["/APPVERSION"].body = "1.3.0\n";
    QSignalSpy found(&um, &UpdateManager::latestVersionFound);
    um.checkForUpdates();
    QTRY_COMPARE(found.count(), 1);

    // Retry takes it, not the 1.2.0 the feed has moved past
    um.retry();
    QTRY_VERIFY(um.state() == UpdateManager::ReadyToInstall && um.readyVersion() == "1.3.0");
    QCOMPARE(m_server->count(sums12), 1);
}

void TestYayc::e2e_feedDisagreesWithSums()
{
    REQUIRE_E2E();
    QVERIFY(downloadInFirstSession());
    setFeed(feed("1.1.0", QByteArray(64, 'f'))); // the site no longer agrees
    const int downloads = m_server->count(assetPath());

    UpdateManager um(e2eConfig());
    um.resumeDownloaded();
    um.applyUpdate();
    // Discarded. Its one re-download stops before any package byte, skipped
    // for this session, silently
    QTRY_COMPARE(um.unavailableVersion(), QStringLiteral("1.1.0"));
    QCOMPARE(um.state(), UpdateManager::Idle);
    QVERIFY(um.lastError().isEmpty());
    QVERIFY(!QFileInfo::exists(cached()));
    QCOMPARE(m_server->count(assetPath()), downloads);
    QCOMPARE(readFile(installRoot() + "/yayc"), QByteArray("old"));
}

void TestYayc::e2e_feedBehindAppVersion()
{
    REQUIRE_E2E();
    m_server->routes["/APPVERSION"].body = "1.2.0\n"; // the feed is still about 1.1.0
    UpdateManager um(e2eConfig());
    um.setAutoUpdate(true);
    um.checkForUpdates();
    QTRY_VERIFY(m_server->count(feedPath()) == 1 && um.state() == UpdateManager::Idle);
    QVERIFY(um.lastError().isEmpty());
    QVERIFY(um.unavailableVersion().isEmpty()); // retried, not skipped
    QCOMPARE(m_server->count("/releases/download/yayc-v1.1.0/SHA256SUMS"), 0);

    addRelease("1.2.0"); // the site catches up
    um.checkForUpdates();
    QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
    QCOMPARE(um.readyVersion(), QStringLiteral("1.2.0"));
}

void TestYayc::e2e_feedUnreadable_data()
{
    QTest::addColumn<QByteArray>("how");
    for (const char *how : {"404", "503", "not JSON", "http URL"})
        QTest::newRow(how) << QByteArray(how);
}

void TestYayc::e2e_feedUnreadable()
{
    REQUIRE_E2E();
    QFETCH(QByteArray, how);
    breakFeed(how);
    UpdateManager um(e2eConfig());
    um.startUpdate("1.1.0");
    QTRY_VERIFY(m_server->count(feedPath()) == 1 && um.state() == UpdateManager::Idle);
    QVERIFY(um.lastError().isEmpty()); // silent: the next check retries
    QVERIFY(um.unavailableVersion().isEmpty());
    QCOMPARE(m_server->count(sumsPath()), 0);
    QCOMPARE(m_server->count(assetPath()), 0);
    QVERIFY(!QFileInfo::exists(cached()) && !QFileInfo::exists(cached() + ".part"));
}

void TestYayc::e2e_feedUnreadableAtVerify_data()
{
    e2e_feedUnreadable_data();
}

void TestYayc::e2e_feedUnreadableAtVerify()
{
    REQUIRE_E2E();
    QFETCH(QByteArray, how);
    QVERIFY(downloadInFirstSession());
    breakFeed(how);
    const int checksums = m_server->count(sumsPath());
    {
        // Automatic: silent, the package stays for a later try
        UpdateManager um(e2eConfig());
        um.setAutoUpdate(true);
        QSignalSpy changed(&um, &UpdateManager::stateChanged);
        um.resumeDownloaded(); // Ready → Verifying → Ready
        QTRY_VERIFY(changed.count() >= 3);
        QCOMPARE(um.state(), UpdateManager::ReadyToInstall);
        QVERIFY(um.lastError().isEmpty());
    }
    // Manual: as in a network outage. The package passed both sources at its download
    UpdateManager um(e2eConfig());
    um.resumeDownloaded();
    um.applyUpdate();
    QTRY_COMPARE(um.state(), UpdateManager::NotVerified);
    QVERIFY(um.lastError().contains("update feed"));
    QVERIFY(QFileInfo::exists(cached()));
    QCOMPARE(m_server->count(sumsPath()), checksums);
    um.applyUnverified();
    QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
}

void TestYayc::e2e_noChecksumsInFeed()
{
    REQUIRE_E2E();
    // A release without SHA256SUMS never installs: retried, no error
    QJsonObject f = feed("1.1.0", pkgSha256());
    f["checksums_url"] = QJsonValue::Null;
    setFeed(f);
    UpdateManager um(e2eConfig());
    um.startUpdate("1.1.0");
    QTRY_VERIFY(m_server->count(feedPath()) == 1 && um.state() == UpdateManager::Idle);
    QVERIFY(um.lastError().isEmpty());
    QVERIFY(um.unavailableVersion().isEmpty());
    QCOMPARE(m_server->count(sumsPath()), 0);
    QCOMPARE(m_server->count(assetPath()), 0);
}

void TestYayc::e2e_feedNewerThanReady()
{
    REQUIRE_E2E();
    QVERIFY(downloadInFirstSession()); // 1.1.0
    addRelease("1.2.0");               // the feed moves on
    const int downloads = m_server->count(assetPath());

    UpdateManager um(e2eConfig());
    QSignalSpy found(&um, &UpdateManager::latestVersionFound);
    um.resumeDownloaded();
    um.applyUpdate();
    // 1.1.0 is discarded; its one re-download stops at the feed
    QTRY_VERIFY(um.state() == UpdateManager::Idle && um.readyVersion().isEmpty());
    QVERIFY(um.lastError().isEmpty());
    // A click: the feed's version is offered next
    QCOMPARE(found.count(), 1);
    QCOMPARE(found.at(0).at(0).toString(), QStringLiteral("1.2.0"));
    QVERIFY(!QFileInfo::exists(cached()));
    QCOMPARE(m_server->count(assetPath()), downloads);

    // The next check finds 1.2.0
    m_server->routes["/APPVERSION"].body = "1.2.0\n";
    um.setAutoUpdate(true);
    um.checkForUpdates();
    QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
    QCOMPARE(um.readyVersion(), QStringLiteral("1.2.0"));
}

void TestYayc::cleanupWaitsForHelper()
{
#ifndef Q_OS_LINUX
    QSKIP("needs processStartToken()");
#endif
    // Windows helper-swap: the helper swaps after YAYC exits, unguarded by the lock
    QVERIFY(QDir().mkpath(installRoot() + ".old"));
    QVERIFY(QDir().mkpath(stagingDir()));
    QVERIFY(writeLiveHelper());
    QVERIFY(writeFile(backupMarker(), "1", false));
    QVERIFY(markRanWell());

    UpdateConfig c = e2eConfig();
    c.updateMethod = UpdateConfig::HelperSwap;
    c.cleanupDelayMs = 0;
    {
        UpdateManager pending(c);
        QTest::qWait(200);
    }
    QVERIFY(QFileInfo::exists(installRoot() + ".old"));
    QVERIFY(QFileInfo::exists(stagingDir()));
    QVERIFY(QFileInfo::exists(helperScript()));

    // The helper died (killed at logoff) and left its script: an orphan
    QVERIFY(writeHelper(deadPid(), "1"));
    UpdateManager done(c);
    QTRY_VERIFY(!QFileInfo::exists(installRoot() + ".old"));
    QVERIFY(!QFileInfo::exists(stagingDir()));
    QVERIFY(!QFileInfo::exists(helperScript()));
    QVERIFY(!QFileInfo::exists(helperPid()));
}

void TestYayc::cleanupAppImageLeftovers()
{
#ifndef Q_OS_UNIX
    QSKIP("AppImage is Linux only");
#endif
    // The backup and a staged image of an earlier session go; the live one stays
    const QString image = m_tmp->filePath("apps/yayc.AppImage");
    QVERIFY(writeFile(image, "img", true));
    QVERIFY(writeFile(image + ".old", "old", true));
    QVERIFY(writeFile(image + ".new", "new", true));
    UpdateConfig c = e2eConfig();
    c.installType = UpdateConfig::LinuxAppImage;
    c.updateMethod = UpdateConfig::ReplaceAppImage;
    c.appImagePath = image;
    c.cleanupDelayMs = 0;
    {
        // A .old without the updater's marker is the user's
        UpdateManager um(c);
        QTRY_VERIFY(!QFileInfo::exists(image + ".new"));
        QCOMPARE(readFile(image + ".old"), QByteArray("old"));
    }
    QVERIFY(writeFile(image + ".old.yayc-backup", "1", false));
    {
        // Marked, but this version never ran well yet: its way back stays
        UpdateManager um(c);
        QTest::qWait(100);
        QCOMPARE(readFile(image + ".old"), QByteArray("old"));
        um.markGoodStart();
    }
    UpdateManager um(c);
    QTRY_VERIFY(!QFileInfo::exists(image + ".old"));
    QVERIFY(!QFileInfo::exists(image + ".old.yayc-backup"));
    QCOMPARE(readFile(image), QByteArray("img"));
}

void TestYayc::e2e_carryOverUserFiles()
{
    REQUIRE_E2E();
    // User data in the install dir, plus a library 1.0.0 shipped and 1.1.0 dropped
    QVERIFY(writeFile(installRoot() + "/portable.ini", "settings", false));
    QVERIFY(QDir().mkpath(installRoot() + "/data/bookmarks"));
    QVERIFY(writeFile(installRoot() + "/data/bookmarks/a.yayc", "bm", false));
    QVERIFY(writeFile(installRoot() + "/data.bin", "user copy", false)); // the package has one
    QVERIFY(writeFile(installRoot() + "/libold.so", "old lib", false));
#ifdef Q_OS_UNIX
    QVERIFY(QFile::link("data", installRoot() + "/data-link"));
#endif

    UpdateManager um(e2eConfig());
    QVERIFY(stageInSession(um));
    QVERIFY(!um.finishOnExit());

    QCOMPARE(readFile(installRoot() + "/yayc"), QByteArray("new"));
    QCOMPARE(readFile(installRoot() + "/portable.ini"), QByteArray("settings"));
    QCOMPARE(readFile(installRoot() + "/data/bookmarks/a.yayc"), QByteArray("bm"));
    QVERIFY(readFile(installRoot() + "/data.bin") != QByteArray("user copy")); // the package wins
    QVERIFY(!QFileInfo::exists(installRoot() + "/libold.so"));
#ifdef Q_OS_UNIX
    QCOMPARE(QFileInfo(installRoot() + "/data-link").readSymLink(), QStringLiteral("data"));
#endif
}

void TestYayc::e2e_carryOverFailureKeepsInstall()
{
    REQUIRE_E2E();
#ifdef Q_OS_UNIX
    if (::geteuid() == 0)
        QSKIP("root reads any file");
    const QString ini = installRoot() + "/portable.ini";
    QVERIFY(writeFile(ini, "settings", false));
    QVERIFY(QFile::setPermissions(ini, QFileDevice::Permissions())); // unreadable

    UpdateManager um(e2eConfig());
    QVERIFY(stageInSession(um));
    um.finishOnExit();
    QFile::setPermissions(ini, QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    // Not carried over: no swap, and the next session is told
    QCOMPARE(readFile(installRoot() + "/yayc"), QByteArray("old"));
    QCOMPARE(readFile(ini), QByteArray("settings"));
    QVERIFY(!QFileInfo::exists(installRoot() + ".old"));
    QVERIFY(readFile(installErrorFile()).contains("portable.ini"));
#else
    QSKIP("Unix permissions");
#endif
}

void TestYayc::e2e_userBackupKept()
{
    REQUIRE_E2E();
    // A backup the user made by hand, before unpacking a new tarball
    QVERIFY(QDir().mkpath(installRoot() + ".old"));
    QVERIFY(writeFile(installRoot() + ".old/bookmarks.yayc", "mine", false));
    {
        UpdateManager um(e2eConfig());
        QVERIFY(stageInSession(um));
        um.finishOnExit();
    }
    // No swap; the user is told
    QCOMPARE(readFile(installRoot() + "/yayc"), QByteArray("old"));
    QCOMPARE(readFile(installRoot() + ".old/bookmarks.yayc"), QByteArray("mine"));
    QVERIFY(!QFileInfo::exists(backupMarker()));
    QVERIFY(!QFileInfo::exists(installRoot() + "/.yayc-backup"));
    QVERIFY(readFile(installErrorFile()).contains("was not made by the updater"));

    // Nor does a later cleanup touch it
    UpdateConfig c = e2eConfig();
    c.cleanupDelayMs = 0;
    UpdateManager next(c);
    next.resumeDownloaded();
    QVERIFY(next.lastError().contains("was not made by the updater"));
    QTRY_VERIFY(!QFileInfo::exists(stagingDir()));
    QCOMPARE(readFile(installRoot() + ".old/bookmarks.yayc"), QByteArray("mine"));
}

void TestYayc::compareSemver_data()
{
    QTest::addColumn<QString>("v1");
    QTest::addColumn<QString>("v2");
    QTest::addColumn<int>("expected");

    QTest::newRow("equal")          << "1.0.0"  << "1.0.0"  <<  0;
    QTest::newRow("major less")     << "1.0.0"  << "2.0.0"  << -1;
    QTest::newRow("major greater")  << "3.0.0"  << "2.0.0"  <<  1;
    QTest::newRow("minor less")     << "1.2.0"  << "1.3.0"  << -1;
    QTest::newRow("minor greater")  << "1.4.0"  << "1.3.0"  <<  1;
    QTest::newRow("patch less")     << "1.0.1"  << "1.0.2"  << -1;
    QTest::newRow("patch greater")  << "1.0.3"  << "1.0.2"  <<  1;
    QTest::newRow("two segments")   << "1.4"    << "1.3"    <<  1;
    QTest::newRow("empty vs ver")   << ""       << "1.0.0"  << -1;
    QTest::newRow("ver vs empty")   << "1.0.0"  << ""       <<  1;
    QTest::newRow("both empty")     << ""       << ""       <<  0;
}

void TestYayc::compareSemver()
{
    QFETCH(QString, v1);
    QFETCH(QString, v2);
    QFETCH(int, expected);

    YaycUtilities utils;
    QCOMPARE(utils.compareSemver(v1, v2), expected);
}

void TestYayc::runAsync_threads()
{
    QObject context;
    QThread *workThread = nullptr;
    QThread *doneThread = nullptr;
    QString result;
    UpdateFetcher::runAsync(&context,
                            [&]() { workThread = QThread::currentThread(); return QStringLiteral("r"); },
                            [&](const QString &r) { doneThread = QThread::currentThread(); result = r; });
    QTRY_VERIFY(doneThread != nullptr);
    QVERIFY(workThread != nullptr);
    QVERIFY(workThread != QThread::currentThread());
    QCOMPARE(doneThread, QThread::currentThread());
    QCOMPARE(result, QStringLiteral("r"));
}

void TestYayc::runAsync_contextDeleted()
{
    bool called = false;
    auto *context = new QObject;
    UpdateFetcher::runAsync(context, []() { return QString(); },
                            [&](const QString &) { called = true; });
    // Deleted before the main loop runs: the queued result must be dropped
    delete context;
    QThreadPool::globalInstance()->waitForDone();
    QCoreApplication::processEvents();
    QVERIFY(!called);
}

void TestYayc::sha256File()
{
    QVERIFY(writeFile(m_tmp->filePath("abc"), "abc", false));
    // FIPS 180-2 test vector for "abc"
    QCOMPARE(UpdateFetcher::sha256File(m_tmp->filePath("abc")),
             QStringLiteral("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    QVERIFY(UpdateFetcher::sha256File(m_tmp->filePath("missing")).isEmpty());
}

void TestYayc::isValidVersion_data()
{
    QTest::addColumn<QString>("version");
    QTest::addColumn<bool>("valid");

    QTest::newRow("release")          << "1.9.0"        << true;
    QTest::newRow("multi-digit")      << "10.20.300"    << true;
    QTest::newRow("empty")            << ""             << false;
    QTest::newRow("two segments")     << "1.9"          << false;
    QTest::newRow("four segments")    << "1.9.0.1"      << false;
    QTest::newRow("v prefix")         << "v1.9.0"       << false;
    QTest::newRow("pre-release")      << "1.9.0-rc1"    << false;
    QTest::newRow("trailing newline") << "1.9.0\n"      << false;
    QTest::newRow("path traversal")   << "../1.9.0"     << false;
    QTest::newRow("non-ascii digits") << QStringLiteral("\u0661.\u0662.\u0663") << false;
}

void TestYayc::isValidVersion()
{
    QFETCH(QString, version);
    QFETCH(bool, valid);
    QCOMPARE(UpdateManager::isValidVersion(version), valid);
}

void TestYayc::shellQuote_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("plain")        << "/a/b.deb"    << "'/a/b.deb'";
    QTest::newRow("empty")        << ""            << "''";
    QTest::newRow("space")        << "/a b/c.deb"  << "'/a b/c.deb'";
    QTest::newRow("single quote") << "/it's/x.deb" << "'/it'\\''s/x.deb'";
    QTest::newRow("expansion")    << "/$HOME/`id`" << "'/$HOME/`id`'";
}

void TestYayc::shellQuote()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QCOMPARE(UpdateLinux::shellQuote(input), expected);
}



void TestYayc::selfBuiltNeverUpdates()
{
    // Test builds have no YAYC_DIST_BUILD, exactly like a user's own build
    UpdateManager um;
    QCOMPARE(um.installType(), UpdateConfig::SelfBuilt);
    QVERIFY(!um.updateSupported());
    QCOMPARE(um.updateMethod(), UpdateConfig::NoMethod);

    um.startUpdate(QStringLiteral("99.0.0"));
    QCOMPARE(um.state(), UpdateManager::Failed);
    QVERIFY(!um.lastError().isEmpty());
    QVERIFY(!um.canStage());

    // The toggle cannot start anything on an unsupported build
    um.setAutoUpdate(true);
    um.startAutoInstall();
    QCOMPARE(um.state(), UpdateManager::Failed);

    QVERIFY(um.manualInstallCommand().isEmpty());
}

void TestYayc::applyUpdateNeedsReadyPackage()
{
    UpdateManager um;
    QSignalSpy changed(&um, &UpdateManager::stateChanged);
    um.applyUpdate();
    um.applyUnverified();
    um.retry();
    um.cancelDownload();
    // All ignored, none failed: nothing verified is waiting
    QCOMPARE(changed.count(), 0);
    QCOMPARE(um.state(), UpdateManager::Idle);
}

void TestYayc::isListedByDpkg()
{
    const QString list = m_tmp->filePath("yayc.list");
    // Same shape as /var/lib/dpkg/info/<pkg>.list; last line without newline
    QVERIFY(writeFile(list, "/.\n/opt\n/opt/yayc\n/opt/yayc/yayc\n/opt/yayc/run.sh\n/usr/bin/yayc", false));

    QVERIFY(UpdateConfig::isListedByDpkg(list, "/opt/yayc/yayc"));
    QVERIFY(UpdateConfig::isListedByDpkg(list, "/usr/bin/yayc"));
    QVERIFY(!UpdateConfig::isListedByDpkg(list, "/opt/yayc/yay"));       // prefix only
    QVERIFY(!UpdateConfig::isListedByDpkg(list, "/opt/yayc/yayc.old"));  // longer
    QVERIFY(!UpdateConfig::isListedByDpkg(list, "/home/u/yayc/yayc"));
    QVERIFY(!UpdateConfig::isListedByDpkg(list, ""));
    QVERIFY(!UpdateConfig::isListedByDpkg(m_tmp->filePath("missing.list"), "/opt/yayc/yayc"));

    const QString empty = m_tmp->filePath("empty.list");
    QVERIFY(writeFile(empty, "", false));
    QVERIFY(!UpdateConfig::isListedByDpkg(empty, "/opt/yayc/yayc"));
}

void TestYayc::sigtermRequestsQuit()
{
#ifdef Q_OS_UNIX
    YaycUtilities utils;
    QSignalSpy quit(&utils, &YaycUtilities::quitRequested);
    utils.watchQuitSignals();
    // If the handler is missing, this kills the test run: also a failure
    ::raise(SIGTERM);
    QTRY_COMPARE(quit.count(), 1);
#else
    QSKIP("SIGTERM handling is Unix only");
#endif
}

void TestYayc::quitSignalRestoresPreviousHandler()
{
#ifdef Q_OS_UNIX
    struct sigaction dummy {};
    dummy.sa_handler = dummySigtermHandler;
    sigemptyset(&dummy.sa_mask);
    struct sigaction saved {};
    QCOMPARE(::sigaction(SIGTERM, &dummy, &saved), 0);
    {
        YaycUtilities utils;
        utils.watchQuitSignals();
        struct sigaction armed {};
        ::sigaction(SIGTERM, nullptr, &armed);
        QVERIFY(armed.sa_handler != dummySigtermHandler);
    }
    // Destroyed: what was there before comes back (e.g. QtTest's handler)
    struct sigaction after {};
    ::sigaction(SIGTERM, nullptr, &after);
    QVERIFY(after.sa_handler == dummySigtermHandler);
    ::sigaction(SIGTERM, &saved, nullptr);
#else
    QSKIP("SIGTERM handling is Unix only");
#endif
}

void TestYayc::isYaycDir()
{
    QVERIFY(UpdateConfig::isYaycDir("/home/u/yayc_linux"));
    QVERIFY(UpdateConfig::isYaycDir("C:/Users/u/YAYC-v1.9.0-win-x64-portable/"));
    // A flat zip extracted into a shared folder: never swapped, never cleaned
    QVERIFY(!UpdateConfig::isYaycDir("C:/Users/u/Downloads"));
    QVERIFY(!UpdateConfig::isYaycDir("/home/u/yayc/bin"));
    QVERIFY(!UpdateConfig::isYaycDir(QString()));
}

void TestYayc::assetNames_data()
{
    // The names the release CI uploads
    QTest::addColumn<UpdateConfig::InstallType>("type");
    QTest::addColumn<QString>("arch");
    QTest::addColumn<QString>("name");
    QTest::newRow("AppImage x86_64") << UpdateConfig::LinuxAppImage << "x86_64" << "yayc-1.9.0-x86_64.AppImage";
    QTest::newRow("AppImage arm64") << UpdateConfig::LinuxAppImage << "arm64" << "yayc-1.9.0-aarch64.AppImage";
    QTest::newRow("tarball x86_64") << UpdateConfig::LinuxTarball << "x86_64" << "yayc-1.9.0-linux-x86_64.tar.xz";
    QTest::newRow("tarball arm64") << UpdateConfig::LinuxTarball << "arm64" << "yayc-1.9.0-linux-aarch64.tar.xz";
    QTest::newRow("deb x86_64") << UpdateConfig::LinuxDeb << "x86_64" << "yayc_1.9.0_amd64.deb";
    QTest::newRow("deb arm64") << UpdateConfig::LinuxDeb << "arm64" << "yayc_1.9.0_arm64.deb";
    QTest::newRow("portable x86_64") << UpdateConfig::WindowsPortable << "x86_64" << "yayc-v1.9.0-win-x64-portable.zip";
    QTest::newRow("portable arm64") << UpdateConfig::WindowsPortable << "arm64" << "yayc-v1.9.0-win-arm64-portable.zip";
    QTest::newRow("installer x86_64") << UpdateConfig::WindowsInstalled << "x86_64" << "yayc-v1.9.0-win-x64.exe";
    QTest::newRow("installer arm64") << UpdateConfig::WindowsInstalled << "arm64" << "yayc-v1.9.0-win-arm64.exe";
    QTest::newRow("macOS") << UpdateConfig::MacOSBundle << "arm64" << "yayc-1.9.0-macos-universal.dmg";
    QTest::newRow("no artifact") << UpdateConfig::LinuxTarball << "riscv64" << "";
}

void TestYayc::assetNames()
{
    QFETCH(UpdateConfig::InstallType, type);
    QFETCH(QString, arch);
    QFETCH(QString, name);
    UpdateConfig c;
    c.installType = type;
    c.arch = arch;
    QCOMPARE(c.assetName("1.9.0"), name);
}

void TestYayc::writeBytesUntranslated()
{
    // --print-manifest output is compared byte for byte by the release CI.
    // Windows opens stdio in text mode: \n would become \r\n
    const QString path = m_tmp->filePath("out.txt");
    const QByteArray data("yayc\r\nlib/a.so\nplugins/b.dll\n");
    std::FILE *f = std::fopen(QFile::encodeName(path).constData(), "w"); // text mode
    QVERIFY(f);
    QVERIFY(UpdateInstall::writeBytes(f, data));
    std::fclose(f);
    QCOMPARE(readFile(path), data);
}

void TestYayc::carryOverDirs()
{
    const QString oldRoot = m_tmp->filePath("old");
    const QString newRoot = m_tmp->filePath("new");
    QVERIFY(QDir().mkpath(oldRoot + "/plugins/gone"));    // the package's
    QVERIFY(QDir().mkpath(oldRoot + "/history"));         // the user's, empty
    QVERIFY(QDir().mkpath(newRoot));
    QVERIFY(writeFile(oldRoot + "/plugins/gone/a.so", "x", false));
    // A read-only user dir still gets its files
    QVERIFY(QDir().mkpath(oldRoot + "/ro"));
    QVERIFY(writeFile(oldRoot + "/ro/f.txt", "keep", false));
    const auto readOnlyDir = QFileDevice::ReadOwner | QFileDevice::ExeOwner;
    QVERIFY(QFile::setPermissions(oldRoot + "/ro", readOnlyDir));
    const auto privateDir = QFileDevice::ReadOwner | QFileDevice::WriteOwner
            | QFileDevice::ExeOwner;
    QVERIFY(QFile::setPermissions(oldRoot + "/history", privateDir));

    const QSet<QString> shipped{UpdateInstall::manifestKey("plugins/gone/a.so")};
    QCOMPARE(UpdateInstall::carryOverUserFiles(oldRoot, newRoot, shipped), QString());
    QVERIFY(QFileInfo(newRoot + "/history").isDir());
#ifdef Q_OS_UNIX
    // Owner / group / other bits only: Qt adds the "User" ones when reading
    QCOMPARE((QFile::permissions(newRoot + "/history") & 0x7077).toInt(), privateDir.toInt());
#endif
    // Package dirs are not recreated empty
    QVERIFY(!QFileInfo::exists(newRoot + "/plugins"));
    QCOMPARE(readFile(newRoot + "/ro/f.txt"), QByteArray("keep"));
#ifdef Q_OS_UNIX
    QCOMPARE((QFile::permissions(newRoot + "/ro") & 0x7077).toInt(), readOnlyDir.toInt());
#endif
    // Writable again, so the temp dirs can go
    for (const QString &d : {oldRoot + "/ro", newRoot + "/ro", m_tmp->filePath("new2/ro")})
        QFile::setPermissions(d, privateDir);

#ifdef Q_OS_UNIX
    // A dir that cannot be listed would lose its files with .old: no swap
    if (::geteuid() != 0) {
        QVERIFY(QDir().mkpath(oldRoot + "/locked"));
        QVERIFY(QFile::setPermissions(oldRoot + "/locked", QFileDevice::Permissions()));
        const QString err = UpdateInstall::carryOverUserFiles(oldRoot, m_tmp->filePath("new2"),
                                                              shipped);
        QFile::setPermissions(oldRoot + "/locked", privateDir);
        QVERIFY(err.contains("locked"));
    }
#endif
}

void TestYayc::readManifest()
{
    const QString path = m_tmp->filePath("yayc-manifest.txt");
    QVERIFY(writeFile(path, "yayc\r\nlib/libQt6Core.so.6\n\nplugins\\platforms\\a.so\n", false));
    const QSet<QString> files = UpdateInstall::readManifest(path);
    QCOMPARE(files.size(), 3);
    QVERIFY(files.contains(UpdateInstall::manifestKey("lib/libQt6Core.so.6")));
    QVERIFY(files.contains(UpdateInstall::manifestKey("plugins/platforms/a.so")));
    QVERIFY(UpdateInstall::readManifest(m_tmp->filePath("missing")).isEmpty());
    // No list: user files cannot be told from old libraries, so no swap
    QVERIFY(!UpdateInstall::carryOverUserFiles(installRoot(), m_tmp->filePath("new"), {}).isEmpty());
}

void TestYayc::isSafeToRemoveBackup()
{
    // Not executable yet: the live install is not proven intact
    const QString exe = m_tmp->filePath("yayc");
    QVERIFY(writeFile(exe, "#!/bin/sh\n", false));
    QVERIFY(!UpdateInstall::isSafeToRemoveBackup(exe));

    QVERIFY(writeFile(exe, "#!/bin/sh\n", true));
    QVERIFY(UpdateInstall::isSafeToRemoveBackup(exe));

    QVERIFY(!UpdateInstall::isSafeToRemoveBackup(m_tmp->filePath("missing")));
    QVERIFY(!UpdateInstall::isSafeToRemoveBackup(m_tmp->path()));   // a dir, not the binary
    QVERIFY(!UpdateInstall::isSafeToRemoveBackup(QString()));
}

void TestYayc::copyFile()
{
    const QString src = m_tmp->filePath("src.bin");
    const QString dst = m_tmp->filePath("dst.bin");

    // Larger than one 1 MiB copy chunk, not a multiple of it
    QByteArray data(3 * 1024 * 1024 + 17, Qt::Uninitialized);
    for (int i = 0; i < data.size(); ++i)
        data[i] = char(i * 31);
    QVERIFY(writeFile(src, data, false));
    QVERIFY(writeFile(dst, QByteArray(10 * 1024 * 1024, 'x'), false)); // longer: must be truncated

    QVERIFY(UpdateInstall::copyFile(src, dst));
    QCOMPARE(readFile(dst), data);

    QVERIFY(!UpdateInstall::copyFile(m_tmp->filePath("missing"), dst));

    // Stopped (e.g. at quit): fails, the copy stays incomplete
    const std::atomic_bool stop{true};
    QVERIFY(!UpdateInstall::copyFile(src, dst, &stop));
    QVERIFY(QFileInfo(dst).size() < data.size());
}

void TestYayc::runningAppImage_data()
{
    QTest::addColumn<QString>("appImage");
    QTest::addColumn<QString>("appDir");
    QTest::addColumn<QString>("exePath");
    QTest::addColumn<QString>("expected");

    const QString img = QStringLiteral("/home/u/Apps/yayc.AppImage");
    QTest::newRow("inside mount") << img << "/tmp/.mount_yaycAb12" << "/tmp/.mount_yaycAb12/usr/bin/yayc" << img;
    QTest::newRow("trailing slash") << img << "/tmp/.mount_yaycAb12/" << "/tmp/.mount_yaycAb12/usr/bin/yayc" << img;
    // Inherited from a parent AppImage (terminal, IDE): not ours
    QTest::newRow("deb under other image") << "/home/u/Apps/ide.AppImage" << "/tmp/.mount_ideXy"
                                           << "/opt/yayc/yayc" << "";
    QTest::newRow("prefix trick") << img << "/tmp/.mount_yayc" << "/tmp/.mount_yaycAb12/usr/bin/yayc" << "";
    QTest::newRow("no APPDIR") << img << "" << "/tmp/.mount_yaycAb12/usr/bin/yayc" << "";
    QTest::newRow("no APPIMAGE") << "" << "/tmp/.mount_yaycAb12" << "/tmp/.mount_yaycAb12/usr/bin/yayc" << "";
}

void TestYayc::runningAppImage()
{
    QFETCH(QString, appImage);
    QFETCH(QString, appDir);
    QFETCH(QString, exePath);
    QFETCH(QString, expected);
    QCOMPARE(UpdateConfig::runningAppImage(appImage, appDir, exePath), expected);
}

void TestYayc::partMeta()
{
    const QString path = m_tmp->filePath("x.part.meta");

    QVERIFY(UpdateFetcher::writePartMeta(path, {"\"abc123\"", "Wed, 21 Oct 2026 07:28:00 GMT"}));
    const auto meta = UpdateFetcher::readPartMeta(path);
    QCOMPARE(meta.etag, QByteArray("\"abc123\""));
    QCOMPARE(meta.lastModified, QByteArray("Wed, 21 Oct 2026 07:28:00 GMT"));
    QCOMPARE(meta.ifRange(), QByteArray("\"abc123\""));

    // If-Range takes no weak ETag: the date is used instead
    const UpdateFetcher::PartMeta weak{"W/\"abc\"", "Wed, 21 Oct 2026 07:28:00 GMT"};
    QCOMPARE(weak.ifRange(), QByteArray("Wed, 21 Oct 2026 07:28:00 GMT"));
    const UpdateFetcher::PartMeta weakOnly{"W/\"abc\"", ""};
    QVERIFY(weakOnly.ifRange().isEmpty());

    // Corrupt or missing meta: no validator, so no resume
    QVERIFY(writeFile(path, "{not json", false));
    QVERIFY(UpdateFetcher::readPartMeta(path).ifRange().isEmpty());
    QVERIFY(UpdateFetcher::readPartMeta(m_tmp->filePath("missing")).ifRange().isEmpty());
}

void TestYayc::parseUpdateFeed_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<QString>("asset");
    QTest::addColumn<int>("outcome");
    QTest::addColumn<QString>("assetUrl"); // Ok only

    using O = UpdateFetcher::Outcome;
    const QString appImage = "yayc-1.1.0-x86_64.AppImage";
    const QString https = "https://example.org/r/yayc-v1.1.0/yayc-1.1.0-x86_64.AppImage";
    const QString local = "http://127.0.0.1:8000/releases/download/yayc-v1.1.0/yayc-1.1.0-x86_64.AppImage";
    const auto quoted = [](const QString &s) { return '"' + s.toLatin1() + '"'; };
    const auto row = [&](const char *name, const QByteArray &json, O outcome,
                         const QString &assetUrl = {}, const QString &asset = {}) {
        QTest::newRow(name) << json << (asset.isEmpty() ? appImage : asset) << int(outcome)
                            << assetUrl;
    };

    row("valid, unknown fields ignored", feedJson(), O::Ok, https);
    row("uppercase hex", feedJson({{"@SHA@", quoted(QString(64, 'A'))}}), O::Ok, https);
    row("http under releasesUrl", feedJson({{"@URL@", quoted(local)}}), O::Ok, local);
    // Unusable: Refused
    row("not JSON", "<html>", O::Refused);
    row("not an object", "[]", O::Refused);
    row("schema 2", feedJson({{"@SCHEMA@", "2"}}), O::Refused);
    row("tag != version", feedJson({{"@TAG@", "yayc-v1.1.1"}}), O::Refused);
    row("version not X.Y.Z", feedJson({{"@VERSION@", "1.1"}, {"@TAG@", "yayc-v1.1"}}), O::Refused);
    row("checksums_url http elsewhere",
        feedJson({{"@CHECKSUMS@", "\"http://example.org/SHA256SUMS\""}}), O::Refused);
    row("url http elsewhere", feedJson({{"@URL@", "\"http://example.org/x\""}}), O::Refused);
    row("url file:", feedJson({{"@URL@", "\"file:///tmp/x\""}}), O::Refused);
    row("url null", feedJson({{"@URL@", "null"}}), O::Refused);
    // Another version, or a release that cannot be checked: Unpublished
    row("feed older", feedJson({{"@VERSION@", "1.0.9"}, {"@TAG@", "yayc-v1.0.9"}}),
        O::Unpublished);
    row("feed newer", feedJson({{"@VERSION@", "1.2.0"}, {"@TAG@", "yayc-v1.2.0"}}),
        O::Unpublished);
    row("checksums_url null", feedJson({{"@CHECKSUMS@", "null"}}), O::Unpublished);
    // No trusted hash for this package: NotListed
    row("no entry", feedJson(), O::NotListed, {}, "yayc-1.1.0-linux-x86_64.tar.xz");
    row("sha256 null", feedJson({{"@SHA@", "null"}}), O::NotListed);
    row("sha256 not hex", feedJson({{"@SHA@", quoted(QString(64, 'g'))}}), O::NotListed);
    row("sha256 too short", feedJson({{"@SHA@", quoted(QString(63, 'a'))}}), O::NotListed);
}

void TestYayc::parseUpdateFeed()
{
    QFETCH(QByteArray, json);
    QFETCH(QString, asset);
    QFETCH(int, outcome);
    QFETCH(QString, assetUrl);
    const auto e = UpdateFetcher::parseUpdateFeed(json, "1.1.0", asset,
                                                  "http://127.0.0.1:8000/releases/download");
    QCOMPARE(int(e.outcome), outcome);
    const bool ok = outcome == int(UpdateFetcher::Outcome::Ok);
    QCOMPARE(e.reason.isEmpty(), ok); // every other outcome says why
    if (!ok)
        return;
    QCOMPARE(e.sha256, QString(64, 'a'));
    QCOMPARE(e.assetUrl.toString(), assetUrl);
    QCOMPARE(e.checksumsUrl.toString(), QStringLiteral("https://example.org/r/yayc-v1.1.0/SHA256SUMS"));
}

void TestYayc::windowsHelperScript()
{
    const QString swap = UpdateWindows::buildHelperScript(
                "1.9.0", "C:/Apps/yayc/yayc.exe", "C:/Apps/yayc", "C:/Apps/yayc.staging/yayc", QString(),
                QString(), {});
    // Waits for every YAYC of this install (running\\<pid>.lock), not a pid
    QVERIFY(!swap.contains("tasklist"));
    QVERIFY(swap.contains("for %%f in (\"running\\*.lock\") do call :probe \"%%~f\"\r\n"));
    QVERIFY(swap.contains("2>nul 9>>\"%~1\" type nul || set busy=1\r\n"));
    QVERIFY(swap.contains("if %waits% lss 60 goto wait\r\ngoto skip\r\n"));
    QVERIFY(swap.indexOf(":ready") < swap.indexOf("\r\nmove "));
    // Gave up: no swap, no error, no relaunch (a YAYC runs)
    const qsizetype skip = swap.indexOf("\r\n:skip\r\n");
    QVERIFY(skip > 0 && swap.indexOf("start \"\"", skip) < 0);
    QVERIFY(!swap.contains("certutil")); // no admin step
    // UTF-8 file, and out of YAYC's working directory before any move
    QVERIFY(swap.indexOf("chcp.com\" 65001") < swap.indexOf("\r\nmove "));
    QVERIFY(swap.indexOf("cd /d \"%~dp0\"") < swap.indexOf("\r\nmove "));
    QVERIFY(swap.contains("move \"C:\\Apps\\yayc\" \"C:\\Apps\\yayc.old\""));
    QVERIFY(swap.contains("move \"C:\\Apps\\yayc.staging\\yayc\" \"C:\\Apps\\yayc\""));
    QVERIFY(!swap.contains("SILENT"));
    // Relaunch only when called with 1 (a restart), or asked by a start
    // that quit for the helper; never on a plain quit. In the install dir,
    // as a normal restart: relative arguments still resolve
    QVERIFY(swap.contains("if exist \"relaunch-request\" goto relaunch\r\n"
                          "if not \"%~1\"==\"1\" goto end\r\n:relaunch\r\n"
                          "del /q \"relaunch-request\" >nul 2>&1\r\n"
                          "start \"\" /D \"C:\\Apps\\yayc\" \"C:\\Apps\\yayc\\yayc.exe\"\r\n"));
    // No parenthesised blocks: %tries% must be read on every pass. A for
    // has only its file set in ( )
    QString noFor = swap;
    noFor.remove("for %%f in (\"running\\*.lock\") do call :probe");
    QVERIFY(!noFor.contains('('));
    // An old backup goes first (move would nest into it), only if the
    // updater made it; no swap if it stays
    QVERIFY(swap.contains("set \"err=yayc.old was not made by the updater: move it away, then retry\""));
    QVERIFY(swap.indexOf("if not exist \"C:\\Apps\\yayc.old\\.yayc-backup\" goto fail")
            < swap.indexOf("rmdir /s /q \"C:\\Apps\\yayc.old\""));
    QVERIFY(swap.indexOf("rmdir") < swap.indexOf("\r\nmove "));
    // Recorded before the move; unrecorded when no backup is left
    QVERIFY(swap.indexOf(">\"C:\\Apps\\yayc\\.yayc-backup\" echo 1.9.0")
            < swap.indexOf("move \"C:\\Apps\\yayc\" \"C:\\Apps\\yayc.old\""));
    // A record that cannot be written: no swap (an unrecorded .old blocks later ones)
    const qsizetype recorded = swap.indexOf(">\"C:\\Apps\\yayc\\.yayc-backup\" echo 1.9.0");
    const qsizetype checked = swap.indexOf("if not exist \"C:\\Apps\\yayc\\.yayc-backup\" goto fail", recorded);
    QVERIFY(recorded > 0 && checked > recorded);
    QVERIFY(checked < swap.indexOf("move \"C:\\Apps\\yayc\" \"C:\\Apps\\yayc.old\""));
    QVERIFY(swap.contains("if %tries% geq 30 goto unrecord"));
    QVERIFY(swap.contains("move \"C:\\Apps\\yayc.old\" \"C:\\Apps\\yayc\" >nul 2>&1\r\n"
                          "if %errorlevel% equ 0 goto unrecord"));
    QVERIFY(swap.indexOf(":unrecord\r\ndel /q \"C:\\Apps\\yayc\\.yayc-backup\"") < swap.indexOf(":fail"));
    // Failures leave a record for the next session; success removes it
    QVERIFY(swap.contains(">\"last-install-error.json\" echo {\"version\":\"1.9.0\",\"error\":\"%err%\"}"));
    QVERIFY(swap.contains("del /q \"last-install-error.json\""));
    QVERIFY(swap.contains("if %errorlevel% equ 0 goto done"));
    QVERIFY(swap.contains("if %errorlevel% equ 0 goto moved"));
    // Journal: the step, written before each move (a resume reads it)
    QVERIFY(swap.indexOf(">\"swap-journal\" echo aside\r\n")
            < swap.indexOf("move \"C:\\Apps\\yayc\" \"C:\\Apps\\yayc.old\""));
    QVERIFY(swap.indexOf(">\"swap-journal\" echo in\r\n")
            < swap.indexOf("move \"C:\\Apps\\yayc.staging\\yayc\" \"C:\\Apps\\yayc\""));
    QVERIFY(swap.indexOf(">\"swap-journal\" echo aside") < swap.indexOf(">\"swap-journal\" echo in"));
    // Argument 2 (RunOnce at logon): straight to the resume, which checks
    // journal and disk: from the start, the second move, done or rolled back
    QVERIFY(swap.indexOf("if \"%~1\"==\"2\" goto resume\r\n") < swap.indexOf(":start\r\n"));
    QVERIFY(swap.contains("2>nul set /p step=<\"swap-journal\"\r\n"));
    QVERIFY(swap.contains(":resumeaside\r\nif exist \"C:\\Apps\\yayc\\yayc.exe\" goto start\r\n"
                          "if exist \"C:\\Apps\\yayc.old\\yayc.exe\" goto moved\r\ngoto broken\r\n"));
    QVERIFY(swap.contains("if exist \"C:\\Apps\\yayc\\yayc.exe\" if not exist"
                          " \"C:\\Apps\\yayc.staging\\yayc\\yayc.exe\" goto done\r\n"));
    // Both moves retry (antivirus, OneDrive); the rollback too
    QVERIFY(swap.contains("if %tries% geq 30 goto rollback"));
    QVERIFY(swap.contains("if %tries% geq 30 goto broken"));
    // No tree in place: a note next to it, an error without a path (JSON:
    // no backslash), and RunOnce and this file stay for the next logon
    const qsizetype broken = swap.indexOf("\r\n:broken\r\n");
    QVERIFY(broken > 0);
    QVERIFY(swap.contains("set \"err=YAYC is not in its folder: see yayc-UPDATE-FAILED.txt next to it\"\r\n"));
    QVERIFY(swap.indexOf(">\"C:\\Apps\\yayc-UPDATE-FAILED.txt\" echo ", broken) > broken);
    const qsizetype brokenEnd = swap.indexOf("exit /b 1\r\n", broken);
    QVERIFY(brokenEnd > broken);
    QVERIFY(!swap.mid(broken, brokenEnd - broken).contains("reg.exe"));
    // RunOnce goes at every other end; the note after a success
    const QString regDelete = "\"%SystemRoot%\\System32\\reg.exe\" delete"
            " \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce\" /v \""
            + UpdateWindows::runOnceName("C:/Apps/yayc") + "\" /f >nul 2>&1\r\n";
    QCOMPARE(swap.count(regDelete), 2); // :finish, :skip
    QVERIFY(swap.indexOf("del /q \"C:\\Apps\\yayc-UPDATE-FAILED.txt\"") > swap.indexOf(":done"));
    // "!": Windows deletes it after the run, so a killed resume runs again
    QVERIFY(UpdateWindows::runOnceName("C:/Apps/yayc").startsWith("!YAYC update "));
    // No live tree (journal lost): the disk decides before any rmdir of .old
    QVERIFY(swap.indexOf(":ready\r\nif not exist \"C:\\Apps\\yayc\\yayc.exe\" goto resumeaside\r\n") > 0);
    QVERIFY(swap.indexOf("goto resumeaside", swap.indexOf(":ready")) < swap.indexOf("rmdir"));
    // A partly deleted .old gets its marker back
    QVERIFY(swap.contains("rmdir /s /q \"C:\\Apps\\yayc.old\"\r\n"
                          "set \"err=cannot remove the previous backup\"\r\n"
                          "if not exist \"C:\\Apps\\yayc.old\" goto mark\r\n"
                          ">\"C:\\Apps\\yayc.old\\.yayc-backup\" echo 1.9.0\r\ngoto fail\r\n"));
    // The pid file goes before the relaunch check: a start after it runs
    QVERIFY(swap.indexOf("del /q \"yayc-update.pid\"") > 0
            && swap.indexOf("del /q \"yayc-update.pid\"") < swap.indexOf("if exist \"relaunch-request\""));
    QCOMPARE(UpdateWindows::runOnceName("C:/Apps/yayc"), UpdateWindows::runOnceName("C:/Apps/yayc"));
    QVERIFY(UpdateWindows::runOnceName("C:/Apps/yayc") != UpdateWindows::runOnceName("C:/Other/yayc"));
    // Gave up waiting: the "did not finish" YAYC wrote goes, nothing failed
    QVERIFY(swap.indexOf("del /q \"last-install-error.json\"", skip) > skip);
    // ping: timeout needs a console stdin
    QVERIFY(!swap.contains("timeout"));
    QVERIFY(swap.contains("\"%SystemRoot%\\System32\\ping.exe\" -n 2 127.0.0.1 >nul"));
    // The staged tree lost the exe since the stage: no swap, a record
    QVERIFY(swap.contains("set \"err=staged tree does not contain yayc.exe\""));
    QVERIFY(swap.indexOf("if not exist \"C:\\Apps\\yayc.staging\\yayc\\yayc.exe\" goto fail")
            < swap.indexOf("rmdir"));

    const QString sha = QString("ab").repeated(32);
    const QString setup = UpdateWindows::buildHelperScript(
                "1.9.0", "C:/Program Files/YAYC/yayc.exe", "C:/Program Files/YAYC", QString(),
                "C:/Users/u/cache/yayc-v1.9.0-win-x64.exe", sha,
                {"-c", "C:/my \"cfg\".ini", "100%"});
    QVERIFY(setup.contains("\"C:\\Users\\u\\cache\\yayc-v1.9.0-win-x64.exe\" /SILENT /NORESTART"));
    // Admin runs only the checked file: hashed again right before, else a
    // record. Through a file: a pipe would expand %% twice
    QVERIFY(setup.contains("\"%SystemRoot%\\System32\\certutil.exe\" -hashfile"
                           " \"C:\\Users\\u\\cache\\yayc-v1.9.0-win-x64.exe\" SHA256"
                           " >\"yayc-update.sha256\" 2>nul\r\n"));
    QVERIFY(setup.contains("\"%SystemRoot%\\System32\\find.exe\" /i \"" + sha
                           + "\" \"yayc-update.sha256\" >nul\r\n"));
    QVERIFY(setup.contains("del /q \"yayc-update.sha256\""));
    QVERIFY(setup.indexOf("certutil") < setup.indexOf("/SILENT"));
    QVERIFY(setup.indexOf("if %errorlevel% equ 0 goto verified")
            < setup.indexOf("set \"err=the installer changed after its checksum check\""));
    QVERIFY(!setup.contains("\r\nmove "));
    // A crash (negative code) is a failure too
    QVERIFY(setup.indexOf("/SILENT") < setup.indexOf("if %errorlevel% equ 0 goto done"));
    QVERIFY(setup.contains("set \"err=the installer failed or was cancelled\""));
    // A planted find.bat in the working dir (updates/) is never run: every
    // external tool by path, no "if not errorlevel" (true for negative codes)
    static const QRegularExpression bareTool(
            QStringLiteral("(?<!System32\\\\)\\b(chcp|ping|tasklist|find|certutil|reg)\\b"));
    for (const QString &script : {swap, setup}) {
        QVERIFY2(!bareTool.match(script).hasMatch(),
                 qPrintable(bareTool.match(script).captured(0)));
        QVERIFY(!script.contains("errorlevel 1"));
        // Every {name} filled (debug: fill() also asserts every value used);
        // no template note, no bare \n
        static const QRegularExpression placeholder(QStringLiteral("\\{[A-Za-z_]\\w*\\}"));
        QVERIFY2(!placeholder.match(script).hasMatch(),
                 qPrintable(placeholder.match(script).captured(0)));
        QVERIFY(!script.startsWith("::") && !script.contains("\n::"));
        QCOMPARE(script.count("\n"), script.count("\r\n"));
        // In a .cmd, set resets errorlevel: a test right after the command
        const QStringList lines = script.split("\r\n");
        for (qsizetype i = 1; i < lines.size(); ++i) {
            if (lines.at(i).startsWith("if %errorlevel%"))
                QVERIFY2(!lines.at(i - 1).startsWith("set "), qPrintable(lines.at(i)));
        }
    }

    // A % in a path stays literal
    const QString pct = UpdateWindows::buildHelperScript(
                "1.9.0", "C:/100%/yayc/yayc.exe", "C:/100%/yayc", "C:/100%/yayc.staging/yayc",
                QString(), QString(), {});
    QVERIFY(pct.contains("move \"C:\\100%%\\yayc\" \"C:\\100%%\\yayc.old\""));
    QVERIFY(pct.contains("start \"\" /D \"C:\\100%%\\yayc\" \"C:\\100%%\\yayc\\yayc.exe\""));
    // A value is not filled again: a {name} in a path stays as it is
    const QString braces = UpdateWindows::buildHelperScript(
                "1.9.0", "C:/{exe}/yayc.exe", "C:/{exe}", "C:/{newRoot}/yayc", QString(),
                QString(), {"{version}"});
    QVERIFY(braces.contains("move \"C:\\{exe}\" \"C:\\{exe}.old\""));
    QVERIFY(braces.contains("move \"C:\\{newRoot}\\yayc\" \"C:\\{exe}\""));
    QVERIFY(braces.contains("start \"\" /D \"C:\\{exe}\" \"C:\\{exe}\\yayc.exe\" \"{version}\"\r\n"));
    const QString setupBraces = UpdateWindows::buildHelperScript(
                "1.9.0", "C:/yayc/yayc.exe", "C:/yayc", QString(), "C:/{hashFile}/setup.exe",
                sha, {});
    QVERIFY(setupBraces.contains("-hashfile \"C:\\{hashFile}\\setup.exe\" SHA256"
                                 " >\"yayc-update.sha256\""));
    // The relaunch keeps the arguments: \" for a quote, %% for a percent
    QVERIFY(setup.contains("\r\nstart \"\" /D \"C:\\Program Files\\YAYC\" \"C:\\Program Files\\YAYC\\yayc.exe\""
                           " \"-c\" \"C:/my \\\"cfg\\\".ini\" \"100%%\"\r\n"));
    QVERIFY(setup.contains("if %waits% lss 60 goto wait\r\ngoto skip\r\n"));
    QVERIFY(setup.indexOf("del /q \"yayc-update.pid\"") > 0
            && setup.indexOf("del /q \"yayc-update.pid\"") < setup.indexOf("if exist \"relaunch-request\""));
    QVERIFY(setup.indexOf(":ready") < setup.indexOf("certutil"));
    // CommandLineToArgvW: backslashes double only before a quote, so a
    // trailing one cannot escape the closing quote. ^ keeps & literal where
    // the \" left cmd outside quotes
    const QString slashes = UpdateWindows::buildHelperScript(
                "1.9.0", "C:/yayc/yayc.exe", "C:/yayc", "C:/yayc.staging/yayc", QString(),
                QString(), {"-c", "D:\\yayc\\cfg\\", "a\\b", "x\" & y", "a\\\"b & c"});
    // After the odd quotes of the 4th, cmd is inside quotes: no ^ in the 5th
    QVERIFY(slashes.contains("\"C:\\yayc\\yayc.exe\" \"-c\" \"D:\\yayc\\cfg\\\\\" \"a\\b\""
                             " \"x\\\" ^& y\" \"a\\\\\\\"b & c\"\r\n"));
}

void TestYayc::debInstallCommand()
{
#ifndef Q_OS_LINUX
    QSKIP("sh, mktemp and sha256sum as on Linux");
#else
    // The root script without pkexec or sudo; dpkg and pkill only record
    const QString bin = m_tmp->filePath("bin");
    QVERIFY(QDir().mkpath(bin));
    QVERIFY(writeFile(bin + "/dpkg",
                      "#!/bin/sh\necho \"$2\" > \"$DPKG_ARG\"\ncat \"$2\" > \"$DPKG_GOT\"\n", true));
    QVERIFY(writeFile(bin + "/pkill", "#!/bin/sh\necho \"$*\" > \"$PKILL_ARGS\"\n", true));
    const QString pkg = m_tmp->filePath("yayc_1.9.0_amd64.deb");
    const QString sha = QString::fromLatin1(
            QCryptographicHash::hash("the package", QCryptographicHash::Sha256).toHex());
    const QString arg = m_tmp->filePath("dpkg-arg");
    const QString pkillArgs = m_tmp->filePath("pkill-args");
    QString err;
    const auto run = [&](const QStringList &cmd) {
        QFile::remove(arg);
        QFile::remove(pkillArgs);
        QProcess p;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("PATH", bin + ":" + env.value("PATH"));
        env.insert("DPKG_ARG", arg);
        env.insert("DPKG_GOT", m_tmp->filePath("dpkg-got"));
        env.insert("PKILL_ARGS", pkillArgs);
        p.setProcessEnvironment(env);
        p.start(cmd.first(), cmd.mid(1));
        if (!p.waitForFinished())
            return -1;
        err = QString::fromUtf8(p.readAllStandardError());
        return p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
    };
    // The script sets the system PATH: here, ours first, for the fake dpkg
    const QString systemPath = "PATH=/usr/sbin:/usr/bin:/sbin:/bin;";
    const QString testPath = "PATH=" + bin + ":/usr/sbin:/usr/bin:/sbin:/bin;";
    QStringList pkexecArgs = UpdateLinux::debInstallCommand(pkg, sha);
    QVERIFY(pkexecArgs.at(2).startsWith(systemPath));
    pkexecArgs[2].replace(systemPath, testPath);
    // The manual command as the user pastes it, minus sudo
    QString manual = UpdateLinux::manualDebCommand(pkg, sha, "yayc");
    QVERIFY(manual.startsWith("/usr/bin/sudo "));
    QCOMPARE(manual.count(systemPath), 1);
    manual.replace(systemPath, testPath);
    const QStringList pasted{"/bin/sh", "-c", manual.mid(QStringLiteral("/usr/bin/sudo ").size())};

    for (const QStringList &cmd : {pkexecArgs, pasted}) {
        // dpkg gets a copy in a fresh dir, gone afterwards
        QVERIFY(writeFile(pkg, "the package", false));
        QCOMPARE(run(cmd), 0);
        const QString copy = QString::fromUtf8(readFile(arg)).trimmed();
        QVERIFY(copy.startsWith("/tmp/yayc-update."));
        QCOMPARE(readFile(m_tmp->filePath("dpkg-got")), QByteArray("the package"));
        QVERIFY(!QFileInfo::exists(QFileInfo(copy).absolutePath()));
        // pkill only in the manual command, and only after the install
        QCOMPARE(QFileInfo::exists(pkillArgs), cmd == pasted);
        if (cmd == pasted)
            QCOMPARE(readFile(pkillArgs).trimmed(), QByteArray("-x yayc"));

        // The cached file changed after its check: no dpkg, no pkill
        QVERIFY(writeFile(pkg, "swapped", false));
        QVERIFY(run(cmd) != 0);
        QVERIFY(err.contains("the package changed after its checksum check"));
        QVERIFY(!QFileInfo::exists(arg));
        QVERIFY(!QFileInfo::exists(pkillArgs));

        // A link (even to the right bytes) or a FIFO: never read
        const QString real = m_tmp->filePath("real.deb");
        QVERIFY(writeFile(real, "the package", false));
        QVERIFY(QFile::remove(pkg));
        QVERIFY(QFile::link(real, pkg));
        QVERIFY(run(cmd) != 0);
        QVERIFY(err.contains("the package is not a regular file"));
        QVERIFY(!QFileInfo::exists(arg));
        QVERIFY(QFile::remove(pkg));
        QCOMPARE(::mkfifo(QFile::encodeName(pkg).constData(), 0600), 0);
        QVERIFY(run(cmd) != 0); // a reader would block: the test would time out
        QVERIFY(err.contains("the package is not a regular file"));
        QVERIFY(QFile::remove(pkg));
    }
#endif
}

void TestYayc::commitAppImage()
{
#ifdef Q_OS_UNIX
    const QString target = m_tmp->filePath("yayc.AppImage");
    const QString marker = target + ".old.yayc-backup";

    // Nothing staged: refuse, the running image stays
    QVERIFY(writeFile(target, "old", true));
    QVERIFY(!UpdateLinux::commitAppImage(target).isEmpty());
    QCOMPARE(readFile(target), QByteArray("old"));

    // Staged but not executable: refuse too
    QVERIFY(writeFile(target + ".new", "new", false));
    QVERIFY(!UpdateLinux::commitAppImage(target).isEmpty());
    QCOMPARE(readFile(target), QByteArray("old"));

    // A .old the updater did not make: kept, no commit
    QVERIFY(writeFile(target + ".new", "new", true));
    QVERIFY(writeFile(target + ".old", "mine", false));
    QVERIFY(UpdateLinux::commitAppImage(target).contains("not made by the updater"));
    QCOMPARE(readFile(target + ".old"), QByteArray("mine"));
    QCOMPARE(readFile(target), QByteArray("old"));
    QVERIFY(!QFileInfo::exists(marker));

    QVERIFY(QFile::remove(target + ".old"));
    QCOMPARE(UpdateLinux::commitAppImage(target), QString());
    QCOMPARE(readFile(target), QByteArray("new"));
    QCOMPARE(readFile(target + ".old"), QByteArray("old"));
    QVERIFY(!QFileInfo::exists(target + ".new"));
    QVERIFY(QFileInfo::exists(marker));

    // The next one replaces the marked .old
    QVERIFY(writeFile(target + ".new", "newer", true));
    QCOMPARE(UpdateLinux::commitAppImage(target), QString());
    QCOMPARE(readFile(target + ".old"), QByteArray("new"));
#else
    QSKIP("AppImage is Linux only");
#endif
}

void TestYayc::commitDirSwap()
{
    const QString root = m_tmp->filePath("yayc");
    const QString staging = UpdateInstall::stagingDir(root);
    const QString newRoot = staging + "/yayc-1.9.0";
    QVERIFY(QDir().mkpath(root));
    QVERIFY(QDir().mkpath(newRoot));
    QVERIFY(writeFile(root + "/yayc", "old", true));

    // A new tree without our binary is never swapped in
    QVERIFY(!UpdateInstall::commitDirSwap(root, newRoot, "yayc").isEmpty());
    QCOMPARE(readFile(root + "/yayc"), QByteArray("old"));
    QVERIFY(!QFileInfo::exists(root + ".old"));

    // A .old the updater did not make (a backup by hand): kept, no swap
    QVERIFY(writeFile(newRoot + "/yayc", "new", true));
    QVERIFY(QDir().mkpath(root + ".old"));
    QVERIFY(writeFile(root + ".old/data", "mine", false));
    QVERIFY(UpdateInstall::commitDirSwap(root, newRoot, "yayc")
            .contains("not made by the updater"));
    QCOMPARE(readFile(root + ".old/data"), QByteArray("mine"));
    QCOMPARE(readFile(root + "/yayc"), QByteArray("old"));
    QVERIFY(!QFileInfo::exists(root + "/.yayc-backup"));

    QVERIFY(QDir(root + ".old").removeRecursively());
    QCOMPARE(UpdateInstall::commitDirSwap(root, newRoot, "yayc"), QString());
    QCOMPARE(readFile(root + "/yayc"), QByteArray("new"));
    QCOMPARE(readFile(root + ".old/yayc"), QByteArray("old"));
    // Marked inside: the next commit or cleanup may delete it; the live tree has none
    QVERIFY(QFileInfo::exists(root + ".old/.yayc-backup"));
    QVERIFY(!QFileInfo::exists(root + "/.yayc-backup"));
    QVERIFY(!QFileInfo::exists(staging)); // wrapper dir cleaned up

    // The next commit replaces the marked .old
    QVERIFY(QDir().mkpath(newRoot));
    QVERIFY(writeFile(newRoot + "/yayc", "newer", true));
    QCOMPARE(UpdateInstall::commitDirSwap(root, newRoot, "yayc"), QString());
    QCOMPARE(readFile(root + ".old/yayc"), QByteArray("new"));
}

void TestYayc::backupMarkerInLiveTree()
{
    // A .old moved back by hand brings its marker into the live tree. A
    // swap does not carry it over; cleanup takes it out
    QVERIFY(writeFile(installRoot() + "/.yayc-backup", "1.0.0", false));
    QVERIFY(writeFile(installRoot() + "/mine", "data", false));
    const QString newRoot = m_tmp->filePath("new");
    QVERIFY(QDir().mkpath(newRoot));
    QCOMPARE(UpdateInstall::carryOverUserFiles(installRoot(), newRoot, {QStringLiteral("yayc")}),
             QString());
    QVERIFY(QFileInfo::exists(newRoot + "/mine"));
    QVERIFY(!QFileInfo::exists(newRoot + "/.yayc-backup"));

    UpdateConfig c = e2eConfig();
    c.cleanupDelayMs = 0;
    UpdateManager um(c);
    QTRY_VERIFY(!QFileInfo::exists(installRoot() + "/.yayc-backup"));
}

void TestYayc::backupPartlyRemoved()
{
#ifndef Q_OS_UNIX
    QSKIP("permissions as on Unix");
#else
    if (::geteuid() == 0)
        QSKIP("root deletes anything");
    // A dir the user cannot empty (e.g. made by a sudo run): the delete
    // stops halfway. The marker comes back: still the updater's .old
    const QString old = installRoot() + ".old";
    QVERIFY(QDir().mkpath(old + "/locked"));
    QVERIFY(writeFile(old + "/locked/file", "x", false));
    QVERIFY(writeFile(old + "/.yayc-backup", "1", false));
    QVERIFY(QFile::setPermissions(old + "/locked", QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    QVERIFY(!UpdateInstall::removeBackup(old));
    QVERIFY(QFileInfo::exists(old + "/.yayc-backup"));
    QVERIFY(QFile::setPermissions(old + "/locked", QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ExeOwner));
    QVERIFY(UpdateInstall::removeBackup(old));
    QVERIFY(!QFileInfo::exists(old));
#endif
}

void TestYayc::e2e_retryOfflineKeepsError()
{
    REQUIRE_E2E();
    // The install at the last exit failed; the user's Retry (Auto: the
    // toggle is on) finds no network: the error and its way on stay
    QVERIFY(downloadInFirstSession());
    QVERIFY(writeFile(installErrorFile(), R"({"version":"1.1.0","error":"disk full"})", false));
    m_server->routes[sumsPath()].status = 503;
    UpdateManager um(e2eConfig());
    um.setAutoUpdate(true);
    um.resumeDownloaded();
    QVERIFY(um.lastError().contains("disk full"));
    um.retry();
    QTRY_COMPARE(um.state(), UpdateManager::NotVerified);
    QVERIFY(!um.lastError().isEmpty());
}

void TestYayc::e2e_restartWithOthers()
{
    REQUIRE_E2E();
    UpdateManager um(e2eConfig());
    um.setAutoUpdate(true);
    um.startUpdate("1.1.0");
    QTRY_COMPARE(um.state(), UpdateManager::ExitPending);
    QVERIFY(um.prepareRestart()); // alone: the restart installs
    // Another YAYC of this install: a restart would not install, and says so
    const QString updates = m_tmp->filePath("cache/updates");
    QVERIFY(QDir().mkpath(updates + "/running"));
    QLockFile other(updates + "/running/999999999.lock");
    QVERIFY(other.tryLock(0));
    QVERIFY(!um.prepareRestart());
    QVERIFY(um.lastError().contains("close the other YAYC windows"));
    QCOMPARE(um.state(), UpdateManager::ExitPending);
}

void TestYayc::updatesDirPerInstall()
{
    const QString a = UpdateConfig::updatesDirFor("/cache", "/apps/yayc");
    QVERIFY(a.startsWith("/cache/updates/"));
    QCOMPARE(a.size(), QString("/cache/updates/").size() + 12);
    QCOMPARE(UpdateConfig::updatesDirFor("/cache", "/apps/yayc/"), a); // same install
    QVERIFY(UpdateConfig::updatesDirFor("/cache", "/apps/yayc-test") != a);
#ifdef Q_OS_WIN
    QCOMPARE(UpdateConfig::updatesDirFor("C:/cache", "C:/Apps/YAYC"),
             UpdateConfig::updatesDirFor("C:/cache", "c:/apps/yayc"));
#endif
}

void TestYayc::testControl()
{
    // As main.cpp registers it: QML reads the method enum from UpdateConfig
    qmlRegisterUncreatableMetaObject(UpdateConfig::staticMetaObject, "yayc", 1, 0,
                                     "UpdateConfig", "enums only");
    QQmlApplicationEngine engine;
    UpdateConfig c;
    c.updatesDir = m_tmp->filePath("cache/updates");
    c.cleanupDelayMs = -1;
    UpdateManager um(c);
    engine.rootContext()->setContextProperty("updateManager", &um);
    engine.loadData(R"(
import QtQuick
import yayc 1.0
Window {
    id: root
    objectName: "win"
    width: 200; height: 100; visible: true
    property int counter: 0
    // um goes before the engine: null then
    property bool noMethod: updateManager !== null && updateManager.updateMethod === UpdateConfig.NoMethod
    function bump(n) { counter += n; return counter }
    Rectangle {
        objectName: "box"
        width: 100; height: 50
        MouseArea { anchors.fill: parent; onClicked: root.counter += 10 }
    }
    Rectangle { objectName: "hidden"; visible: false; width: 10; height: 10 }
}
)");
    QVERIFY(!engine.rootObjects().isEmpty());

    TestControl control(&engine);
    const auto run = [&](const QString &line) {
        QJsonObject reply;
        bool replied = false;
        control.execute(line, [&](const QJsonObject &r) { reply = r; replied = true; });
        if (!QTest::qWaitFor([&]() { return replied; }, 5000))
            reply = {{"ok", false}, {"error", "no reply"}};
        return reply;
    };
    const auto value = [&](const QString &line) {
        const QJsonObject r = run(line);
        return r.value("ok").toBool() ? r.value("value") : QJsonValue(r.value("error"));
    };

    QCOMPARE(value("get updateManager.state").toString(), QString("Idle")); // enum key
    // An enum of another class (UpdateConfig) too: the bench reads it
    QCOMPARE(value("get updateManager.updateMethod").toString(), QString("NoMethod"));
    QCOMPARE(value("get win.noMethod").toBool(), true);
    QVERIFY(!run("call root.bump 5").value("ok").toBool()); // an id, not an objectName
    QCOMPARE(value("call win.bump 5").toInt(), 5);
    QVERIFY(run("set win.counter 1").value("ok").toBool());
    QVERIFY(run("click box").value("ok").toBool());
    QCOMPARE(value("get win.counter").toInt(), 11);
    QCOMPARE(value("visible hidden").toBool(), false);
    QCOMPARE(value("exists nothing").toBool(), false);
    QVERIFY(!run("click hidden").value("ok").toBool());
    QVERIFY(!run("call updateManager.noSuchMethod").value("ok").toBool());
    QCOMPARE(value("call updateManager.cancelDownload"), QJsonValue()); // a C++ invokable
    // um lives on the stack: the call must not hand it to the garbage collector
    QCOMPARE(QJSEngine::objectOwnership(&um), QJSEngine::CppOwnership);
    engine.collectGarbage();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(value("get updateManager.state").toString(), QString("Idle"));

    QTimer::singleShot(200, &engine, [&]() { engine.rootObjects().first()->setProperty("counter", 12); });
    QVERIFY(run("wait win.counter 12 3000").value("ok").toBool());
    QVERIFY(!run("wait win.counter 99 300").value("ok").toBool()); // timeout

    // Over the socket, one JSON line per command
    const QString name = QStringLiteral("yayc-test-control-%1").arg(QCoreApplication::applicationPid());
    QVERIFY(control.listen(name));
    QLocalSocket socket;
    socket.connectToServer(name);
    QVERIFY(socket.waitForConnected(3000));
    socket.write("get win.counter\n");
    QTRY_VERIFY(socket.canReadLine());
    const QJsonObject reply = QJsonDocument::fromJson(socket.readLine()).object();
    QVERIFY(reply.value("ok").toBool());
    QCOMPARE(reply.value("value").toInt(), 12);
}

QTEST_MAIN(TestYayc)
#include "tst_yayc.moc"
