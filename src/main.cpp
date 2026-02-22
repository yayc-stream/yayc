/*
Copyright (C) 2023- YAYC team <yaycteam@gmail.com>

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
#include "qqmlsettings.h"

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
#include <QFileInfo>
#include <QThread>
#include <cstring>

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
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-accessibility --log-level=3");

    int currentExitCode = 0;
    QStringList args;
    QString appFilePath;
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
        args = app.arguments();
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

        QUrl configFileUrl;
        if (parser.isSet(configOption)) {
            QString configFile = parser.value(configOption);
            QFileInfo fi(configFile);
            configFile = fi.absoluteFilePath();
            configFileUrl = QUrl::fromLocalFile(configFile);
            settings.reset(new QSettings(configFile, QSettings::IniFormat));
        }
#ifndef Q_OS_LINUX
        else {
            configFileUrl = QUrl::fromLocalFile(settings->fileName());
        }
#endif


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
#endif

        // for the roles enums
        qmlRegisterUncreatableType<FileSystemModel>("yayc", 1, 0,
                                                   "FileSystemModel", "Cannot create a FileSystemModel instance.");

        qmlRegisterSingletonType(QUrl("qrc:/WebBrowsingProfiles.qml"), "yayc", 1, 0, "WebBrowsingProfiles");
        qmlRegisterSingletonType(QUrl("qrc:/WebEngineInternals.qml"), "yayc", 1, 0, "WebEngineInternals");
        qmlRegisterSingletonType(QUrl("qrc:/YaycProperties.qml"), "yayc", 1, 0, "YaycProperties");
        qmlRegisterType<QQmlSettings>("yayc", 1, 0, "YaycSettings");


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

        engine.rootContext()->setContextProperty("utilities", utilities);
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
        engine.load(url);
        currentExitCode = app.exec();
    }
    if ( currentExitCode <=  YaycUtilities::EXIT_CODE_ERASE_SETTINGS) {
        if (currentExitCode == YaycUtilities::EXIT_CODE_ERASE_SETTINGS) {
            QLoggingCategory category("qmldebug");
            qCInfo(category) << "Erasing settings...";

            if (!YaycUtilities::settingsFileToDelete.isEmpty()) {
                QFile::remove(YaycUtilities::settingsFileToDelete);
                qCInfo(category) << "Removed" << YaycUtilities::settingsFileToDelete;
            }
        }

#ifdef Q_OS_MACOS
        // On macOS, relaunch the .app bundle via 'open' for reliable restart
        QString bundlePath = appFilePath;
        int contentsIdx = bundlePath.indexOf("/Contents/MacOS/");
        if (contentsIdx != -1) {
            bundlePath.truncate(contentsIdx);
            QProcess::startDetached("open", QStringList{"-n", bundlePath, "--args"} + args.mid(1));
        } else {
            QProcess::startDetached(appFilePath, args.mid(1));
        }
#elif defined(Q_OS_WIN)
        // On Windows, wait briefly for the exe lock to be released before restarting
        QThread::msleep(500);
        QProcess::startDetached(appFilePath, args.mid(1), QCoreApplication::applicationDirPath());
#else
        QProcess::startDetached(appFilePath, args.mid(1));
#endif
        return 0;
    }
    return currentExitCode;
}
