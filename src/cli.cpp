#include "cli.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSaveFile>
#include <QTextStream>

#include "engine.h"
#include "sheetstore.h"

namespace {

void printUsage(QTextStream &out) {
    out << "Numbrero — fast plain-text variable calculator\n\n"
        << "Usage:\n"
        << "  numbrero [open] [file...]               Open the GUI (optionally opening files)\n"
        << "  numbrero eval [file|-] [--json]         Evaluate a sheet or stdin\n"
        << "  numbrero eval -e <expr> [--json]        Evaluate an expression or multiline text\n"
        << "  numbrero check <file> [--json]          Check a sheet for syntax or unit errors\n"
        << "  numbrero sheets [--json]                List saved scratchpad sheets\n"
        << "  numbrero skill install [--dir <path>]   Install the bundled agent skill\n"
        << "  numbrero --help | --version\n";
}

QString kindName(LineKind kind) {
    switch (kind) {
    case LineKind::Empty: return QStringLiteral("empty");
    case LineKind::Heading: return QStringLiteral("heading");
    case LineKind::Comment: return QStringLiteral("comment");
    case LineKind::Note: return QStringLiteral("note");
    case LineKind::Assignment: return QStringLiteral("assignment");
    case LineKind::Expression: return QStringLiteral("expression");
    case LineKind::Error: return QStringLiteral("error");
    }
    return QStringLiteral("empty");
}

QJsonObject resultToJson(const SheetResult &res) {
    QJsonArray lines;
    for (const LineResult &line : res.lines) {
        QJsonObject obj;
        obj.insert(QStringLiteral("line"), line.lineNumber + 1);
        obj.insert(QStringLiteral("kind"), kindName(line.kind));
        if (!line.variableName.isEmpty())
            obj.insert(QStringLiteral("variable"), line.variableName);
        if (!line.displayText.isEmpty())
            obj.insert(QStringLiteral("result"), line.displayText);
        if (!line.errorText.isEmpty())
            obj.insert(QStringLiteral("error"), line.errorText);
        lines.append(obj);
    }

    QJsonObject vars;
    for (const VariableEntry &var : res.variables)
        vars.insert(var.name, var.displayText);

    QJsonObject root;
    root.insert(QStringLiteral("ok"), !res.hasErrors);
    root.insert(QStringLiteral("lines"), lines);
    root.insert(QStringLiteral("variables"), vars);
    return root;
}

bool readInputText(const QString &fileArg, QString &textOut, QString &errOut) {
    if (fileArg == QStringLiteral("-")) {
        QFile stdinFile;
        if (!stdinFile.open(stdin, QIODevice::ReadOnly)) {
            errOut = QStringLiteral("Failed to read stdin");
            return false;
        }
        textOut = SheetStore::normalizeLineEndings(QString::fromUtf8(stdinFile.readAll()));
        return true;
    }
    QFile file(fileArg);
    if (!file.open(QIODevice::ReadOnly)) {
        errOut = QStringLiteral("Cannot open '%1': %2").arg(fileArg, file.errorString());
        return false;
    }
    textOut = SheetStore::normalizeLineEndings(QString::fromUtf8(file.readAll()));
    return true;
}

} // namespace

int Cli::run(const QStringList &args, QStringList &guiFilesOut, const QString &dataDir) {
    QTextStream out(stdout);
    QTextStream err(stderr);

    if (args.size() <= 1)
        return -1;

    const QString cmd = args.at(1);

    if (cmd == QStringLiteral("-h") || cmd == QStringLiteral("--help") || cmd == QStringLiteral("help")) {
        printUsage(out);
        out.flush();
        return 0;
    }
    if (cmd == QStringLiteral("-v") || cmd == QStringLiteral("--version")) {
        out << "numbrero 0.1.0\n";
        out.flush();
        return 0;
    }
    if (cmd == QStringLiteral("open")) {
        for (int i = 2; i < args.size(); ++i)
            guiFilesOut.push_back(args.at(i));
        return -1;
    }

    if (cmd == QStringLiteral("eval") || cmd == QStringLiteral("check")) {
        const bool checkOnly = (cmd == QStringLiteral("check"));
        bool jsonMode = false;
        bool hasExpr = false;
        QString exprText;
        QString fileArg;

        for (int i = 2; i < args.size(); ++i) {
            const QString &a = args.at(i);
            if (a == QStringLiteral("--json")) {
                jsonMode = true;
            } else if (!checkOnly && (a == QStringLiteral("-e") || a == QStringLiteral("--expr"))) {
                if (i + 1 >= args.size()) {
                    err << "Missing expression after " << a << "\n";
                    return 1;
                }
                hasExpr = true;
                exprText = SheetStore::normalizeLineEndings(args.at(++i));
            } else if (a.startsWith(QLatin1Char('-')) && a != QStringLiteral("-")) {
                err << "Unknown option: " << a << "\n";
                return 1;
            } else if (fileArg.isEmpty()) {
                fileArg = a;
            } else {
                err << "Unexpected argument: " << a << "\n";
                return 1;
            }
        }

        if (checkOnly && fileArg.isEmpty()) {
            err << "Missing file argument for 'check'\n";
            return 1;
        }

        QString text;
        if (hasExpr) {
            text = exprText;
        } else {
            if (fileArg.isEmpty())
                fileArg = QStringLiteral("-");
            QString ioErr;
            if (!readInputText(fileArg, text, ioErr)) {
                err << ioErr << "\n";
                return 1;
            }
        }

        const SheetResult res = Engine::evaluate(text, QLocale::c());
        if (jsonMode) {
            out << QJsonDocument(resultToJson(res)).toJson(QJsonDocument::Compact) << "\n";
            out.flush();
            if (res.hasErrors) {
                for (const LineResult &l : res.lines) {
                    if (l.kind == LineKind::Error)
                        err << "line " << (l.lineNumber + 1) << ": " << l.errorText << "\n";
                }
                return 1;
            }
            return 0;
        }

        for (const LineResult &l : res.lines) {
            if (l.kind == LineKind::Error) {
                err << "line " << (l.lineNumber + 1) << ": " << l.errorText << "\n";
            } else if (!checkOnly && !l.displayText.isEmpty()) {
                if (!l.variableName.isEmpty())
                    out << l.variableName << " = " << l.displayText << "\n";
                else
                    out << l.displayText << "\n";
            }
        }
        out.flush();
        err.flush();
        return res.hasErrors ? 1 : 0;
    }

    if (cmd == QStringLiteral("sheets")) {
        bool jsonMode = false;
        for (int i = 2; i < args.size(); ++i) {
            if (args.at(i) == QStringLiteral("--json"))
                jsonMode = true;
            else {
                err << "Unknown option: " << args.at(i) << "\n";
                return 1;
            }
        }

        SheetStore store(dataDir);
        store.load();
        if (jsonMode) {
            QJsonArray arr;
            for (int i = 0; i < store.sheets().size(); ++i) {
                const Sheet &s = store.sheets().at(i);
                QJsonObject obj;
                obj.insert(QStringLiteral("id"), s.id);
                obj.insert(QStringLiteral("title"), s.title);
                obj.insert(QStringLiteral("active"), i == store.activeIndex());
                if (!s.externalPath.isEmpty())
                    obj.insert(QStringLiteral("externalPath"), s.externalPath);
                arr.append(obj);
            }
            QJsonObject root;
            root.insert(QStringLiteral("sheets"), arr);
            out << QJsonDocument(root).toJson(QJsonDocument::Compact) << "\n";
            out.flush();
            return 0;
        }

        for (int i = 0; i < store.sheets().size(); ++i) {
            const Sheet &s = store.sheets().at(i);
            out << (i == store.activeIndex() ? "* " : "  ") << s.title;
            if (!s.externalPath.isEmpty())
                out << " (" << s.externalPath << ")";
            out << "\n";
        }
        out.flush();
        return 0;
    }

    if (cmd == QStringLiteral("skill")) {
        if (args.size() < 3 || args.at(2) != QStringLiteral("install")) {
            err << "Usage: numbrero skill install [--dir <path>]\n";
            return 1;
        }
        QString targetRoot = QDir::homePath() + QStringLiteral("/.agents/skills");
        for (int i = 3; i < args.size(); ++i) {
            if (args.at(i) == QStringLiteral("--dir") && i + 1 < args.size())
                targetRoot = args.at(++i);
            else {
                err << "Unknown option: " << args.at(i) << "\n";
                return 1;
            }
        }
        QFile src(QStringLiteral(":/skill/SKILL.md"));
        if (!src.open(QIODevice::ReadOnly)) {
            err << "Bundled skill resource missing\n";
            return 1;
        }
        const QString destDir = QDir(targetRoot).filePath(QStringLiteral("numbrero"));
        if (!QDir().mkpath(destDir)) {
            err << "Cannot create directory: " << destDir << "\n";
            return 1;
        }
        const QString destFile = QDir(destDir).filePath(QStringLiteral("SKILL.md"));
        QSaveFile outFile(destFile);
        if (!outFile.open(QIODevice::WriteOnly)) {
            err << "Cannot write skill: " << outFile.errorString() << "\n";
            return 1;
        }
        const QByteArray data = src.readAll();
        if (outFile.write(data) != data.size() || !outFile.commit()) {
            err << "Failed to write skill: " << outFile.errorString() << "\n";
            return 1;
        }
        out << "Installed skill to " << destFile << "\n";
        out.flush();
        return 0;
    }

    if (!cmd.startsWith(QLatin1Char('-'))) {
        // Treat bare file arguments as files to open in the GUI.
        for (int i = 1; i < args.size(); ++i)
            guiFilesOut.push_back(args.at(i));
        return -1;
    }

    err << "Unknown option: " << cmd << "\n";
    return 1;
}
