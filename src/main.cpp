/*
Copyright (C) 2023- YAYC team <yaycteam@gmail.com>

This work is licensed under the terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/ or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

In addition to the above,
- The use of this work for training artificial intelligence is prohibited for both commercial and non-commercial use.
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

namespace {
bool isPlasmaSession() {
#ifdef Q_OS_LINUX
    QProcess plasmaRunning;
    QStringList arguments;
    arguments << "ksmserver";
    plasmaRunning.start("/usr/bin/pidof", arguments);
    plasmaRunning.setStandardOutputFile(QProcess::nullDevice());
    plasmaRunning.setStandardErrorFile(QProcess::nullDevice());

    if(!plasmaRunning.waitForFinished())
        return false; // Not found or pidof does not work

    return !plasmaRunning.exitCode();
#else
    return false;
#endif
}

bool findExecLinux(const QString &name) {
    QProcess findProcess;
    QStringList arguments;
    arguments << name;
    findProcess.start("which", arguments);
    findProcess.setReadChannel(QProcess::ProcessChannel::StandardOutput);

    if(!findProcess.waitForFinished())
        return false; // Not found or which does not work

    QString retStr(findProcess.readAll());

    retStr = retStr.trimmed();

    QFile file(retStr);
    QFileInfo check_file(file);
    if (check_file.exists() && check_file.isFile())
        return true; // Found!
    else
        return false; // Not found!
}
} // namespace

int main(int argc, char *argv[])
{
    int currentExitCode = 0;
    QStringList args;
    {
        YaycUtilities::appstartTS = QDateTime::currentDateTimeUtc();
        QCoreApplication::setOrganizationName("YAYC");
        QCoreApplication::setApplicationName("yayc");

        QScopedPointer<QSettings> settings(new QSettings);


        QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
        QtWebEngineQuick::initialize();

        QGuiApplication app(argc, argv);
        app.setWindowIcon(QIcon(":/images/yayc-alt.png"));
        args = app.arguments();

        // Create command line parser
        QCommandLineParser parser;
        parser.setApplicationDescription("YAYC");
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
        if (parser.isSet(configOption)) {
            QString configFile = parser.value(configOption);
            settings.reset(new QSettings(configFile, QSettings::NativeFormat));
        }

#if defined(Q_OS_LINUX)
        qputenv("QT_QPA_PLATFORMTHEME", QByteArrayLiteral("gtk3"));
#endif
        qputenv("QT_QUICK_CONTROLS_STYLE", QByteArrayLiteral("Material"));
        qputenv("QT_STYLE_OVERRIDE", QByteArrayLiteral("Material"));

        if (!settings->contains("darkMode") || settings->value("darkMode").toBool()) {
            qputenv("QT_QUICK_CONTROLS_MATERIAL_THEME", QByteArrayLiteral("Dark")); // ToDo: fix text color
        }
        qputenv("QT_QUICK_CONTROLS_MATERIAL_PRIMARY", QByteArrayLiteral("#3d3d3d"));
        qputenv("QT_QUICK_CONTROLS_MATERIAL_ACCENT", QByteArrayLiteral("Red"));
        qputenv("QT_QUICK_CONTROLS_MATERIAL_VARIANT", QByteArrayLiteral("Dense")); // ToDo: add setting

        qInfo("Starting YAYC v%s ...", appVersion().data());
#ifdef QT_NO_DEBUG_OUTPUT
//        QLoggingCategory::setFilterRules(QStringLiteral("*=false\n"
//                                                        "qmldebug=true\n"
//                                                        "*.fatal=true\n"
//                                                        ));
#endif

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

        // for the roles enums
        qmlRegisterUncreatableType<FileSystemModel>("yayc", 1, 0,
                                                    "FileSystemModel", "Cannot create a FileSystemModel instance.");


        YaycUtilities *utilities = new YaycUtilities(&engine);

        engine.rootContext()->setContextProperty("utilities", utilities);
        engine.rootContext()->setContextProperty("appVersion", QString(appVersion()) );
        engine.rootContext()->setContextProperty("repositoryURL", repositoryURL );

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

            QSettings settings;
            settings.clear();
            settings.sync();
        }

        QProcess::startDetached(args[0], args); //application restart
        return 0;
    }
    return currentExitCode;
}
