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

#include "Platform.h"
#include "VideoMetadata.h"
#include "ChannelMetadata.h"
#include "ThumbnailImageProvider.h"
#include "EmptyIconProvider.h"
#include "NoDirSortProxyModel.h"
#include "FileSystemModel.h"
#include "ThumbnailFetcher.h"
#include "RequestInterceptor.h"
#include "YaycUtilities.h"
#include "KeyInterceptor.h"
#include "qqmlsettings.h"
#include "YaycContext.h"
#include "UpdateManager.h"
#include "SingleInstance.h"
#ifdef YAYC_TEST_BUILD
#include "TestControl.h"
#endif

#include <QGuiApplication>
#include <QApplication>
#include <QSettings>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml>
#include <QtWebEngineCore/qtwebenginecoreglobal.h>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#include <QDateTime>
#include <QQuickStyle>
#include <QProcess>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QThread>
#include <QTimer>
#include <cstdio>
#include <cstring>
#include <memory>

namespace {
bool isPlasmaSession() {
#ifdef Q_OS_LINUX
    QProcess plasmaRunning;
    QStringList arguments;
    arguments << "ksmserver";
    plasmaRunning.setStandardOutputFile(QProcess::nullDevice());
    plasmaRunning.setStandardErrorFile(QProcess::nullDevice());
    plasmaRunning.start("/usr/bin/pidof", arguments);

    if(!plasmaRunning.waitForFinished())
        return false; // Not found or pidof does not work

    return !plasmaRunning.exitCode();
#else
    return false;
#endif
}
} // namespace

int main(int argc, char *argv[])
{
    // Handle --help/--version before heavy Qt/WebEngine initialization
    for (int i = 1; i < argc; ++i) {
        // The release scripts compare it with the package they build
        if (strcmp(argv[i], "--print-manifest") == 0) {
            QFile manifest(UpdateInstall::manifestResource);
            if (!manifest.open(QIODevice::ReadOnly))
                return 1;
            return UpdateInstall::writeBytes(stdout, manifest.readAll()) ? 0 : 1;
        }
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0
            || strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            QCoreApplication::setOrganizationName("YAYC");
            QCoreApplication::setApplicationName("yayc");
            QCoreApplication helpApp(argc, argv);
            helpApp.setApplicationVersion(appVersion());
            QCommandLineParser parser;
            parser.setApplicationDescription("YAYC - Yet Another YouTube Client");
            parser.addHelpOption();
            parser.addVersionOption();
            parser.addOption({{"d", "debug"}, "Enable debug mode"});
            parser.addOption({{"c", "config"}, "Configuration file path", "file"});
            parser.addOption({"print-manifest", "Print the files of the release package"});
            parser.process(helpApp); // exits for help/version
            return 0;
        }
    }

    // Must be set before ANY Qt initialization
    // Workaround for Qt 6.10.x accessibility crash (QTBUG-...)
    qputenv("QT_ACCESSIBILITY", "0");
    qputenv("QT_LINUX_ACCESSIBILITY_ALWAYS_ON", "0");
    qputenv("QTWEBENGINE_ENABLE_LINUX_ACCESSIBILITY", "0");
    // qunsetenv("AT_SPI_BUS_ADDRESS");  // Disable AT-SPI D-Bus connection
    // qputenv("NO_AT_BRIDGE", "1");     // Another way to disable AT-SPI
    // --disable-renderer-backgrounding / --disable-backgrounding-occluded-windows:
    // without these, Chromium stops compositing frames for a renderer it considers
    // backgrounded while audio keeps flowing - so switching away from YAYC leaves a
    // frozen picture with continuing sound, and a hover preview has to be unpinned and
    // re-hovered to come back. Costs some idle CPU/battery while YAYC sits in the
    // background, which is the intended trade for a client whose whole point is that
    // playback keeps going.
    static const char *kBackgroundingFlags =
            " --disable-renderer-backgrounding"
            " --disable-backgrounding-occluded-windows";
#ifdef QT_NO_DEBUG_OUTPUT
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
            QByteArray("--disable-accessibility --log-level=3") + kBackgroundingFlags);
#else
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
            QByteArray("--disable-accessibility --log-level=0") + kBackgroundingFlags);
#endif


    int currentExitCode = 0;
    // What a restart passes on: the parsed options, -c as an absolute path,
    // since the relaunch may run in another working dir
    QStringList relaunchArgs;
    QString appFilePath;
    UpdateInstall::ExitPlan updateExitPlan;
    // Until the process ends, through the exit plan: the Windows helper waits for it
    std::unique_ptr<QLockFile> runningLock;
    // The configuration's: through the exit plan, until a restart relaunches
    std::unique_ptr<QLockFile> configLock;
    // As a path string: after an update commit it names the new tree, not .old
    const QString startDir = QDir::currentPath();
    {
        YaycUtilities::appstartTS = QDateTime::currentDateTimeUtc();
        QCoreApplication::setOrganizationName("YAYC");
        QCoreApplication::setApplicationName("yayc");

#ifdef Q_OS_LINUX
        QScopedPointer<QSettings> settings(new QSettings);
#else
        QScopedPointer<QSettings> settings(
            new QSettings(QSettings::IniFormat, QSettings::UserScope, "YAYC", "yayc"));
#endif

        QGuiApplication::setApplicationDisplayName("YAYC");
        QGuiApplication::setDesktopFileName("yayc");

        QtWebEngineQuick::initialize();

        qSetMessagePattern("%{file}:%{line} - %{message}");
        QGuiApplication app(argc, argv);
#ifndef Q_OS_MACOS
        app.setWindowIcon(QIcon(":/images/yayc-alt.png"));
#endif
        // What a restart runs. AppImage: the image file, not its mount, so an
        // applied update takes effect. Read while app exists: it needs it
        appFilePath = UpdateConfig::runningAppImage();
        if (appFilePath.isEmpty())
            appFilePath = app.applicationFilePath();

        // Create command line parser
        QCommandLineParser parser;
        parser.setApplicationDescription("YAYC - Yet Another YouTube Client");
        parser.addHelpOption();
        parser.addVersionOption();

        // Add custom command line options
        QCommandLineOption debugOption(QStringList() << "d" << "debug",
                                       "Enable debug mode");
        parser.addOption(debugOption);

        QCommandLineOption configOption(QStringList() << "c" << "config",
                                        "Configuration file path",
                                        "file");
        parser.addOption(configOption);
        parser.process(app);

        if (parser.isSet(debugOption))
            relaunchArgs << QStringLiteral("-d");
        QUrl configFileUrl;
        if (parser.isSet(configOption)) {
            QString configFile = parser.value(configOption);
            QFileInfo fi(configFile);
            configFile = fi.absoluteFilePath();
            configFileUrl = QUrl::fromLocalFile(configFile);
            settings.reset(new QSettings(configFile, QSettings::IniFormat));
            relaunchArgs << QStringLiteral("-c") << configFile;
        }
#ifndef Q_OS_LINUX
        else {
            configFileUrl = QUrl::fromLocalFile(settings->fileName());
        }
#endif
        if (!parser.positionalArguments().isEmpty())
            relaunchArgs << QStringLiteral("--") << parser.positionalArguments();

        // One process per configuration, as Firefox per profile: a second
        // start brings the first one's window to the front
        SingleInstance singleInstance;
        if (!singleInstance.claim(settings->fileName()))
            return 0;


#if defined(Q_OS_LINUX)
        qputenv("QT_QPA_PLATFORMTHEME", QByteArrayLiteral("gtk3"));
#endif
        qputenv("QT_QUICK_CONTROLS_STYLE", QByteArrayLiteral("Material"));
        qputenv("QT_STYLE_OVERRIDE", QByteArrayLiteral("Material"));

        const bool darkMode = !settings->contains("darkMode") || settings->value("darkMode").toBool();
        // Superseded by Material.theme binding on ApplicationWindow in main.qml
        // if (darkMode) {
        //     qputenv("QT_QUICK_CONTROLS_MATERIAL_THEME", QByteArrayLiteral("Dark"));
        // }
        qputenv("QT_QUICK_CONTROLS_MATERIAL_PRIMARY", QByteArrayLiteral("#3d3d3d"));
        qputenv("QT_QUICK_CONTROLS_MATERIAL_ACCENT", QByteArrayLiteral("Red"));
        qputenv("QT_QUICK_CONTROLS_MATERIAL_VARIANT", QByteArrayLiteral("Dense")); // ToDo: add setting



        qInfo("Starting YAYC v%s ...", appVersion().data());
        qInfo("Configuration: %s", qPrintable(settings->fileName()));
#ifdef QT_NO_DEBUG_OUTPUT
        QLoggingCategory::setFilterRules(QStringLiteral("*=false\n"
                                                        "qmldebug=true\n"
                                                        "*.fatal=true\n"
                                                        ));
#else
        qDebug() << "DEBUG build!";
#endif

        // for the roles enums
        qmlRegisterUncreatableType<FileSystemModel>("yayc", 1, 0,
                                                   "FileSystemModel", "Cannot create a FileSystemModel instance.");

        qmlRegisterSingletonType(QUrl("qrc:/WebBrowsingProfiles.qml"), "yayc", 1, 0, "WebBrowsingProfiles");
        qmlRegisterSingletonType(QUrl("qrc:/WebEngineInternals.qml"), "yayc", 1, 0, "WebEngineInternals");
        qmlRegisterSingletonType(QUrl("qrc:/YaycProperties.qml"), "yayc", 1, 0, "YaycProperties");
        qmlRegisterType<QQmlSettings>("yayc", 1, 0, "YaycSettings");
        // for the UpdateState and Method enums
        qmlRegisterUncreatableType<UpdateManager>("yayc", 1, 0,
                                                  "UpdateManager", "Cannot create an UpdateManager instance.");
        qmlRegisterUncreatableMetaObject(UpdateConfig::staticMetaObject, "yayc", 1, 0,
                                         "UpdateConfig", "Cannot create an UpdateConfig instance.");

        QQmlApplicationEngine engine;
        ThumbnailImageProvider *imageProvider = new ThumbnailImageProvider();
        engine.addImageProvider(QLatin1String("videothumbnail"), imageProvider);
        const QUrl url(QStringLiteral("qrc:/main.qml"));
        QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                         &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);


        FileSystemModel *historyModel =
               new FileSystemModel("historyModel", false, &engine);
        FileSystemModel *fsmodel =
               new FileSystemModel("fileSystemModel", true, &engine);

        engine.rootContext()->setContextProperty("fileSystemModel", fsmodel);
        engine.rootContext()->setContextProperty("historyModel", historyModel);

        YaycUtilities *utilities = new YaycUtilities(&engine);
        // Filter is a no-op unless utilities.keepForegroundIllusion is set
        // (bound from QML settings), so install it unconditionally.
        app.installEventFilter(utilities);
        KeyInterceptor *keyInterceptor = new KeyInterceptor(&engine);
        app.installEventFilter(keyInterceptor);

        YaycContext *yaycContext = new YaycContext(engine, &engine);
        QObject::connect(utilities, &YaycUtilities::languageChanged,
                         yaycContext, &YaycContext::retranslate);

        UpdateConfig updateConfig = UpdateConfig::detect();
        updateConfig.relaunchArgs = relaunchArgs;
        UpdateManager *updateManager = new UpdateManager(updateConfig, &engine);
        // Windows: the helper of an earlier exit needs this install's files
        if (updateManager->deferToHelper())
            return 0;
        if (updateManager->updateSupported())
            runningLock = UpdateInstall::registerRunning(updateConfig.updatesDir);
        // Windows portable: a stage an earlier session left (it ended at
        // logoff) installs now, without the UI; the helper relaunches YAYC
        const bool installStaged = updateManager->resumeStage();
        // A package from an earlier session (also one downloaded with the toggle off)
        updateManager->resumeDownloaded();
        // An automatic install that found no network goes on once it is back
        QObject::connect(utilities, &YaycUtilities::networkFound,
                         updateManager, &UpdateManager::startAutoInstall);

        engine.rootContext()->setContextProperty("utilities", utilities);
        engine.rootContext()->setContextProperty("updateManager", updateManager);
        engine.rootContext()->setContextProperty("keyInterceptor", keyInterceptor);
        engine.rootContext()->setContextProperty("appVersion", QString(appVersion()) );
        engine.rootContext()->setContextProperty("repositoryURL", repositoryURL );
        engine.rootContext()->setContextProperty("configFileUrl", configFileUrl);
        engine.rootContext()->setContextProperty("initialDarkMode", darkMode);

        YaycUtilities::isPlasma = isPlasmaSession();

        QObject::connect(fsmodel, &FileSystemModel::firstInitializationCompleted,
                         [fsmodel, &settings](const QString &path) {
            if (!fsmodel->ready()) {
                if (!settings->contains("debugMode") || !settings->value("debugMode").toBool())
                    return;
                auto modelReadyTS = QDateTime::currentDateTimeUtc();
                auto msecs = YaycUtilities::appstartTS.msecsTo(modelReadyTS);
                QLoggingCategory category("qmldebug");
                qCInfo(category) << "Starting time for "
                                 << path<< " : " << msecs << " ms";
            }
        });
        if (!installStaged)
            engine.load(url);
        // The .old of the version before stays until this one ran a while
        if (!engine.rootObjects().isEmpty())
            QTimer::singleShot(5 * 60 * 1000, updateManager, &UpdateManager::markGoodStart);
        QObject::connect(&singleInstance, &SingleInstance::activationRequested, &engine, [&engine]() {
            const auto roots = engine.rootObjects();
            if (!roots.isEmpty())
                QMetaObject::invokeMethod(roots.first(), "bringToFront");
        });
#ifdef YAYC_TEST_BUILD
        // Update test bench: driven through this socket
        (new TestControl(&engine, &engine))->listen(qEnvironmentVariable("YAYC_TEST_CONTROL"));
#endif
        utilities->watchQuitSignals();
        currentExitCode = installStaged ? YaycUtilities::EXIT_CODE_REBOOT : app.exec();
        updateExitPlan = updateManager->takeExitPlan();
        // A start from now on waits for this process to end, not for a window
        configLock = singleInstance.release();
    }
    const bool restart = currentExitCode <= YaycUtilities::EXIT_CODE_ERASE_SETTINGS;
    // Before the update: its carry-over would copy the file into the new version
    if (currentExitCode == YaycUtilities::EXIT_CODE_ERASE_SETTINGS) {
        QLoggingCategory category("qmldebug");
        qCInfo(category) << "Erasing settings...";

        if (!YaycUtilities::settingsFileToDelete.isEmpty()) {
            QFile::remove(YaycUtilities::settingsFileToDelete);
            qCInfo(category) << "Removed" << YaycUtilities::settingsFileToDelete;
        }
    }
    // After the block: QML settings are on disk now. Commits a staged update
    // (Linux) or starts the Windows helper, which then does the relaunch
    const bool helperRelaunches = UpdateInstall::runExitPlan(updateExitPlan, restart);
    configLock.reset(); // the relaunched process claims it
    if (restart && !helperRelaunches) {

#ifdef Q_OS_MACOS
        // On macOS, relaunch the .app bundle via 'open' for reliable restart
        QString bundlePath = appFilePath;
        int contentsIdx = bundlePath.indexOf("/Contents/MacOS/");
        if (contentsIdx != -1) {
            bundlePath.truncate(contentsIdx);
            QProcess::startDetached("open", QStringList{"-n", bundlePath, "--args"} + relaunchArgs);
        } else {
            QProcess::startDetached(appFilePath, relaunchArgs);
        }
#elif defined(Q_OS_WIN)
        // On Windows, wait briefly for the exe lock to be released before restarting
        QThread::msleep(500);
        QProcess::startDetached(appFilePath, relaunchArgs, QFileInfo(appFilePath).absolutePath());
#else
        // Qt opens the working dir before the fork: a deleted one fails the start
        bool relaunched = false;
        for (const QString &dir : {startDir, QFileInfo(appFilePath).absolutePath(), QDir::homePath()}) {
            relaunched = QFileInfo(dir).isDir() && QProcess::startDetached(appFilePath, relaunchArgs, dir);
            if (relaunched)
                break;
        }
        if (!relaunched)
            return 1;
#endif
        return 0;
    }
    return restart ? 0 : currentExitCode;
}
