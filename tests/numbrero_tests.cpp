#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "cli.h"
#include "engine.h"
#include "filepicker.h"
#include "sheetstore.h"
#include "theme.h"
#include "translator.h"

class FakeFilePicker : public FilePicker {
    Q_OBJECT

public:
    explicit FakeFilePicker(QObject *parent = nullptr) : FilePicker(parent) {}

    void openFile(const QString &title) override {
        lastOpenTitle = title;
        if (!nextOpenPath.isEmpty())
            emit filePicked(nextOpenPath);
    }

    void saveFile(const QString &title, const QString &suggestedName) override {
        lastSaveTitle = title;
        lastSuggestedName = suggestedName;
        if (!nextSavePath.isEmpty())
            emit savePicked(nextSavePath);
    }

    QString lastOpenTitle;
    QString lastSaveTitle;
    QString lastSuggestedName;
    QString nextOpenPath;
    QString nextSavePath;
};

class NumbreroTests : public QObject {
    Q_OBJECT

private slots:
    void engineVariablesAndArithmetic() {
        const QString sheet =
            "apples = 3\n"
            "oranges = 5\n"
            "fruits = apples + oranges\n"
            "let doubled = fruits * 2\n"
            "2 + 3 * 4\n"
            "(2 + 3) * 4\n"
            "2 ^ 3 ^ 2\n"
            "1_000 + 2.5e2\n"
            "10 % 3\n";

        const SheetResult res = Engine::evaluate(sheet, QLocale::c());
        QVERIFY(!res.hasErrors);
        QCOMPARE(res.lines.at(0).displayText, QString("3"));
        QCOMPARE(res.lines.at(1).displayText, QString("5"));
        QCOMPARE(res.lines.at(2).displayText, QString("8"));
        QCOMPARE(res.lines.at(3).displayText, QString("16"));
        QCOMPARE(res.lines.at(4).displayText, QString("14"));
        QCOMPARE(res.lines.at(5).displayText, QString("20"));
        QCOMPARE(res.lines.at(6).displayText, QString("512"));
        QCOMPARE(res.lines.at(7).displayText, QString("1250"));
        QCOMPARE(res.lines.at(8).displayText, QString("1"));
        QCOMPARE(res.variables.size(), 4);
    }

    void enginePercentagesAndBlocks() {
        const QString sheet =
            "# Groceries\n"
            "bread = 4\n"
            "milk = 6\n"
            "sum\n"
            "\n"
            "price = 200\n"
            "20% of price\n"
            "price + 15%\n"
            "price - 25%\n"
            "0.5 in %\n"
            "prev * 2\n"
            "avg(10, 20, 30)\n"
            "count\n";

        const SheetResult res = Engine::evaluate(sheet, QLocale::c());
        QVERIFY(!res.hasErrors);
        QCOMPARE(res.lines.at(0).kind, LineKind::Heading);
        QCOMPARE(res.lines.at(3).displayText, QString("10"));
        QCOMPARE(res.lines.at(6).displayText, QString("40"));
        QCOMPARE(res.lines.at(7).displayText, QString("230"));
        QCOMPARE(res.lines.at(8).displayText, QString("150"));
        QCOMPARE(res.lines.at(9).displayText, QString("50%"));
        QCOMPARE(res.lines.at(10).displayText, QString("100%"));
        QCOMPARE(res.lines.at(11).displayText, QString("20"));
        QCOMPARE(res.lines.at(12).displayText, QString("7"));
    }

    void engineMathFunctionsAndConstants() {
        const QString sheet =
            "sqrt(81)\n"
            "round(pi, 2)\n"
            "min(10, 3, 7)\n"
            "max(10, 3, 7)\n"
            "clamp(15, 0, 10)\n"
            "log(100)\n"
            "log(8, 2)\n"
            "round(ln(e), 4)\n"
            "abs(-42)\n";

        const SheetResult res = Engine::evaluate(sheet, QLocale::c());
        QVERIFY(!res.hasErrors);
        QCOMPARE(res.lines.at(0).displayText, QString("9"));
        QCOMPARE(res.lines.at(1).displayText, QString("3.14"));
        QCOMPARE(res.lines.at(2).displayText, QString("3"));
        QCOMPARE(res.lines.at(3).displayText, QString("10"));
        QCOMPARE(res.lines.at(4).displayText, QString("10"));
        QCOMPARE(res.lines.at(5).displayText, QString("2"));
        QCOMPARE(res.lines.at(6).displayText, QString("3"));
        QCOMPARE(res.lines.at(7).displayText, QString("1"));
        QCOMPARE(res.lines.at(8).displayText, QString("42"));
    }

    void engineUnitsAndConversions() {
        const QString sheet =
            "12 in to cm\n"
            "12 in in cm\n"
            "5 km in mi\n"
            "1 m + 50 cm\n"
            "2 kg in g\n"
            "72 F in C\n"
            "100 C in F\n"
            "0 C in K\n"
            "90 min in h\n"
            "2 GiB in MiB\n"
            "1 GB in MB\n"
            "500 mL + 0.5 L\n";

        const SheetResult res = Engine::evaluate(sheet, QLocale::c());
        QVERIFY(!res.hasErrors);
        QCOMPARE(res.lines.at(0).displayText, QString("30.48 cm"));
        QCOMPARE(res.lines.at(1).displayText, QString("30.48 cm"));
        QVERIFY(res.lines.at(2).displayText.startsWith("3.10685"));
        QCOMPARE(res.lines.at(3).displayText, QString("1.5 m"));
        QCOMPARE(res.lines.at(4).displayText, QString("2000 g"));
        QVERIFY(res.lines.at(5).displayText.startsWith("22.2222"));
        QCOMPARE(res.lines.at(6).displayText, QString("212 °F"));
        QCOMPARE(res.lines.at(7).displayText, QString("273.15 K"));
        QCOMPARE(res.lines.at(8).displayText, QString("1.5 h"));
        QCOMPARE(res.lines.at(9).displayText, QString("2048 MiB"));
        QCOMPARE(res.lines.at(10).displayText, QString("1000 MB"));
        QCOMPARE(res.lines.at(11).displayText, QString("1000 mL"));
    }

    void engineNotesCommentsAndErrors() {
        const QString sheet =
            "// Top comment\n"
            "Monthly shopping list\n"
            "apples = 3 # green\n"
            "5 km + 3 kg\n"
            "10 / 0\n"
            "missingVar + 1\n";

        const SheetResult res = Engine::evaluate(sheet, QLocale::c());
        QVERIFY(res.hasErrors);
        QCOMPARE(res.lines.at(0).kind, LineKind::Comment);
        QCOMPARE(res.lines.at(1).kind, LineKind::Note);
        QCOMPARE(res.lines.at(2).kind, LineKind::Assignment);
        QCOMPARE(res.lines.at(2).displayText, QString("3"));
        QCOMPARE(res.lines.at(3).kind, LineKind::Error);
        QCOMPARE(res.lines.at(4).kind, LineKind::Error);
        QCOMPARE(res.lines.at(5).kind, LineKind::Error);
    }

    void engineCurrenciesImplicitMulAndBases() {
        const QString sheet =
            "apples = 4\n"
            "2(3 + 4)\n"
            "(1 + 2)(3 + 4)\n"
            "round(2pi, 4)\n"
            "2sqrt(9)\n"
            "3 apples\n"
            "$12.50 + $8.20\n"
            "€200 - 15%\n"
            "20% off $150\n"
            "250 kr * 2\n"
            "$100 / $25\n"
            "0xff\n"
            "0b1010_1100\n"
            "255 in hex\n"
            "42 in bin\n"
            "511 in oct\n"
            "0xff in dec\n";

        const SheetResult res = Engine::evaluate(sheet, QLocale::c());
        QVERIFY(!res.hasErrors);
        QCOMPARE(res.lines.at(1).displayText, QString("14"));
        QCOMPARE(res.lines.at(2).displayText, QString("21"));
        QCOMPARE(res.lines.at(3).displayText, QString("6.2832"));
        QCOMPARE(res.lines.at(4).displayText, QString("6"));
        QCOMPARE(res.lines.at(5).displayText, QString("12"));
        QCOMPARE(res.lines.at(6).displayText, QString("$20.7"));
        QCOMPARE(res.lines.at(7).displayText, QString("€170"));
        QCOMPARE(res.lines.at(8).displayText, QString("$120"));
        QCOMPARE(res.lines.at(9).displayText, QString("500 kr"));
        QCOMPARE(res.lines.at(10).displayText, QString("4"));
        QCOMPARE(res.lines.at(11).displayText, QString("255"));
        QCOMPARE(res.lines.at(12).displayText, QString("172"));
        QCOMPARE(res.lines.at(13).displayText, QString("0xff"));
        QCOMPARE(res.lines.at(14).displayText, QString("0b101010"));
        QCOMPARE(res.lines.at(15).displayText, QString("0o777"));
        QCOMPARE(res.lines.at(16).displayText, QString("255"));

        const SheetResult badCurrency = Engine::evaluate("$10 + €10", QLocale::c());
        QVERIFY(badCurrency.hasErrors);
        QCOMPARE(badCurrency.lines.at(0).kind, LineKind::Error);

        const SheetResult badBase = Engine::evaluate("3.5 in hex", QLocale::c());
        QVERIFY(badBase.hasErrors);
        QCOMPARE(badBase.lines.at(0).kind, LineKind::Error);

        const SheetResult prec2 = Engine::evaluate("1 / 3", QLocale::c(), 2);
        QVERIFY(!prec2.hasErrors);
        QCOMPARE(prec2.lines.at(0).displayText, QString("0.33"));
    }

    void backendPrecisionDuplicateAndCopyWithResults() {
        QTemporaryDir dir;
        Backend backend(dir.path());
        backend.setActiveText("x = 1 / 3\nx * 6\n");
        QCOMPARE(backend.precision(), 6);
        QCOMPARE(backend.results().at(0).toMap().value("displayText").toString(), QString("0.333333"));

        backend.setPrecision(2);
        QCOMPARE(backend.precision(), 2);
        QCOMPARE(backend.results().at(0).toMap().value("displayText").toString(), QString("0.33"));

        backend.cyclePrecision();
        QCOMPARE(backend.precision(), 4);

        backend.copySheetWithResults();
        QCOMPARE(QGuiApplication::clipboard()->text(), QString("x = 1 / 3  // 0.3333\nx * 6  // 2\n"));

        backend.duplicateSheet(0);
        QCOMPARE(backend.sheets().size(), 2);
        QVERIFY(backend.activeTitle().endsWith("(copy)"));
        QCOMPARE(backend.activeText(), QString("x = 1 / 3\nx * 6\n"));
    }

    void sheetStoreManagesMultipleSheetsAndFiles() {
        QTemporaryDir dir;
        {
            SheetStore store(dir.path());
            store.load();
            QCOMPARE(store.count(), 1);
            QCOMPARE(store.activeIndex(), 0);

            const int second = store.addSheet("Trip", "gas = 40\nhotel = 120\nsum\n");
            QCOMPARE(second, 1);
            QCOMPARE(store.count(), 2);
            QVERIFY(store.renameSheet(second, "Road Trip"));
            store.saveAllManaged();
        }

        // Reopen from disk and verify state persists.
        SheetStore reloaded(dir.path());
        reloaded.load();
        QCOMPARE(reloaded.count(), 2);
        QCOMPARE(reloaded.activeIndex(), 1);
        QCOMPARE(reloaded.sheets().at(1).title, QString("Road Trip"));
        QVERIFY(reloaded.sheets().at(1).content.contains("hotel = 120"));

        // External file round-trip.
        const QString extPath = dir.filePath("budget.txt");
        QVERIFY(reloaded.saveSheetToExternalFile(1, extPath));
        QVERIFY(QFile::exists(extPath));
        QVERIFY(!reloaded.hasUnsavedExternalSheets());

        reloaded.setSheetContent(1, "gas = 55\n");
        QVERIFY(reloaded.hasUnsavedExternalSheets());
        QVERIFY(reloaded.saveSheet(1));
        QVERIFY(!reloaded.hasUnsavedExternalSheets());
    }

    void backendEvaluatesAndUsesFilePicker() {
        QTemporaryDir dir;
        FakeFilePicker picker;
        Backend backend(dir.path(), &picker);

        backend.setActiveText("x = 12\ny = x * 3\n");
        QCOMPARE(backend.variableCount(), 2);
        QCOMPARE(backend.results().at(1).toMap().value("displayText").toString(), QString("36"));

        const QString saveTarget = dir.filePath("saved.txt");
        picker.nextSavePath = saveTarget;
        backend.saveFileAsDialog();
        QCOMPARE(backend.activeFilePath(), QFileInfo(saveTarget).absoluteFilePath());
        QVERIFY(QFile::exists(saveTarget));
    }

    void themeFallsBackAndReadsOmarchyConfig() {
        QTemporaryDir dir;
        Theme fallbackTheme(dir.path());
        QCOMPARE(fallbackTheme.accent(), QString("#FFD60A"));
        QCOMPARE(fallbackTheme.accentForeground(), QString("black"));

        QDir(dir.path()).mkpath("theme");
        writeFile(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");

        Theme customTheme(dir.path());
        QCOMPARE(customTheme.accent(), QString("#112233"));
        QCOMPARE(customTheme.accentForeground(), QString("white"));

        const QString response = dir.filePath("response.json");
        const QString command = dir.filePath("hyprctl");
        writeFile(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        writeFile(response, "{\"int\":0}");

        Theme roundingTheme(dir.path(), nullptr, command);
        QCOMPARE(roundingTheme.radius(), 0);
        writeFile(response, "{\"int\":8}");
        QTRY_COMPARE(roundingTheme.radius(), 8);
    }

    void translatorFollowsLanguageAndPlurals() {
        QTemporaryDir dir;
        Translator fallback(dir.path() + "/language", ":/i18n");
        QCOMPARE(fallback.language(), QString("en"));
        QCOMPARE(fallback.translate("Main", "%n lines", nullptr, 1), QString("%n line"));
        QCOMPARE(fallback.translate("Main", "%n lines", nullptr, 3), QString("%n lines"));

        writeFile(dir.path() + "/language", "da\n");
        Translator danish(dir.path() + "/language", ":/i18n");
        QCOMPARE(danish.language(), QString("da"));
        QCOMPARE(danish.translate("Main", "Save"), QString("Gem"));
        QCOMPARE(danish.translate("Main", "%n lines", nullptr, 2), QString("%n linjer"));
    }

    void cliEvalCheckAndSkillInstall() {
        QTemporaryDir dir;
        QStringList guiFiles;

        const QString validFile = dir.filePath("valid.txt");
        writeFile(validFile, "apples = 3\noranges = 5\nfruits = apples + oranges\n");
        QCOMPARE(Cli::run({"numbrero", "check", validFile, "--json"}, guiFiles, dir.path()), 0);

        const QString invalidFile = dir.filePath("bad.txt");
        writeFile(invalidFile, "10 / 0\n");
        QCOMPARE(Cli::run({"numbrero", "check", invalidFile}, guiFiles, dir.path()), 1);

        const QString skillDir = dir.filePath("skills");
        QCOMPARE(Cli::run({"numbrero", "skill", "install", "--dir", skillDir}, guiFiles, dir.path()), 0);
        QVERIFY(QFile::exists(skillDir + "/numbrero/SKILL.md"));
    }

private:
    static void writeFile(const QString &path, const QByteArray &contents) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(contents), contents.size());
    }
};

QTEST_MAIN(NumbreroTests)
#include "numbrero_tests.moc"
