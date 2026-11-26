// numbrero — fast plain-text variable calculator scratchpad for Omarchy.

#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "portalfilepicker.h"
#include "theme.h"
#include "translator.h"

namespace {

bool needsHeadlessApp(int argc, char *argv[]) {
    if (argc < 2)
        return false;
    const QString arg1 = QString::fromLocal8Bit(argv[1]);
    return arg1 == QStringLiteral("-h") || arg1 == QStringLiteral("--help") ||
           arg1 == QStringLiteral("help") || arg1 == QStringLiteral("-v") ||
           arg1 == QStringLiteral("--version") || arg1 == QStringLiteral("eval") ||
           arg1 == QStringLiteral("check") || arg1 == QStringLiteral("sheets") ||
           arg1 == QStringLiteral("skill");
}

} // namespace

int main(int argc, char *argv[]) {
    if (needsHeadlessApp(argc, argv)) {
        QCoreApplication cliApp(argc, argv);
        cliApp.setApplicationName(QStringLiteral("numbrero"));
        cliApp.setApplicationVersion(QStringLiteral("0.1.0"));
        QStringList unusedFiles;
        return Cli::run(cliApp.arguments(), unusedFiles);
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("numbrero"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

    // Matches the window to numbrero.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed sombrero icon.
    app.setDesktopFileName(QStringLiteral("numbrero"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("numbrero"),
                                       QIcon(QStringLiteral(":/icon.svg"))));

    QStringList filesToOpen;
    const int cliCode = Cli::run(app.arguments(), filesToOpen);
    if (cliCode >= 0)
        return cliCode;

    QQuickStyle::setStyle(QStringLiteral("Material"));

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    PortalFilePicker picker;
    Backend backend({}, &picker);

    for (const QString &path : filesToOpen)
        backend.openPath(path);

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    QObject::connect(&translator, &Translator::changed, &backend, &Backend::reformatForLocale);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &backend, &Backend::flushPendingSaves);

    engine.rootContext()->setContextProperty(QStringLiteral("translator"), &translator);
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
