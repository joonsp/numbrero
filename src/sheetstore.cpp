#include "sheetstore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

SheetStore::SheetStore(const QString &dataDir)
    : m_dataDir(dataDir.isEmpty()
                    ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                    : dataDir) {
    if (m_dataDir.isEmpty())
        m_dataDir = QDir::homePath() + QStringLiteral("/.local/share/numbrero");
}

QString SheetStore::normalizeLineEndings(QString text) {
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

QString SheetStore::defaultScratchpadContent() {
    return QStringLiteral(
        "# Groceries\n"
        "apples = 3\n"
        "oranges = 5\n"
        "fruits = apples + oranges\n"
        "\n"
        "# Discount & Tax\n"
        "price = 240\n"
        "sale = price - 15%\n"
        "tax = 8% of sale\n"
        "sale + tax\n"
        "\n"
        "# Units\n"
        "5 km in mi\n"
        "72 F in C\n");
}

QString SheetStore::sheetFilePath(const QString &id) const {
    return m_dataDir + QStringLiteral("/sheets/") + id + QStringLiteral(".numbrero");
}

QString SheetStore::nextSheetId() const {
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);
}

QString SheetStore::nextDefaultTitle() const {
    for (int i = 1;; ++i) {
        const QString candidate = QStringLiteral("Sheet %1").arg(i);
        bool exists = false;
        for (const Sheet &s : m_sheets) {
            if (s.title == candidate) {
                exists = true;
                break;
            }
        }
        if (!exists)
            return candidate;
    }
}

bool SheetStore::writeAtomically(const QString &path, const QByteArray &data, QString *errorOut) {
    const QFileInfo info(path);
    QDir().mkpath(info.absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorOut)
            *errorOut = file.errorString();
        return false;
    }
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        if (errorOut)
            *errorOut = file.errorString();
        return false;
    }
    if (!file.commit()) {
        if (errorOut)
            *errorOut = file.errorString();
        return false;
    }
    return true;
}

void SheetStore::setActiveIndex(int index) {
    if (m_sheets.isEmpty()) {
        m_activeIndex = 0;
        return;
    }
    m_activeIndex = qBound(0, index, m_sheets.size() - 1);
}

void SheetStore::setPrecision(int precision) {
    const int clamped = qBound(2, precision, 8);
    if (m_precision == clamped)
        return;
    m_precision = clamped;
    saveIndex();
}

void SheetStore::load() {
    m_sheets.clear();
    m_activeIndex = 0;

    const QString indexPath = m_dataDir + QStringLiteral("/index.json");
    QFile indexFile(indexPath);
    if (indexFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(indexFile.readAll());
        const QJsonObject root = doc.object();
        const QJsonArray arr = root.value(QStringLiteral("sheets")).toArray();

        for (const QJsonValue &val : arr) {
            const QJsonObject obj = val.toObject();
            Sheet s;
            s.id = obj.value(QStringLiteral("id")).toString();
            s.title = obj.value(QStringLiteral("title")).toString();
            s.externalPath = obj.value(QStringLiteral("externalPath")).toString();
            if (s.id.isEmpty())
                continue;

            if (!s.externalPath.isEmpty()) {
                QFile ext(s.externalPath);
                if (!ext.open(QIODevice::ReadOnly))
                    continue;
                s.content = normalizeLineEndings(QString::fromUtf8(ext.readAll()));
                if (s.title.isEmpty())
                    s.title = QFileInfo(s.externalPath).fileName();
            } else {
                QFile managed(sheetFilePath(s.id));
                if (managed.open(QIODevice::ReadOnly))
                    s.content = normalizeLineEndings(QString::fromUtf8(managed.readAll()));
                if (s.title.isEmpty())
                    s.title = QStringLiteral("Scratchpad");
            }
            m_sheets.push_back(s);
        }

        m_activeIndex = root.value(QStringLiteral("activeIndex")).toInt(0);
        m_precision = qBound(2, root.value(QStringLiteral("precision")).toInt(6), 8);
    }

    if (m_sheets.isEmpty()) {
        Sheet initial;
        initial.id = nextSheetId();
        initial.title = QStringLiteral("Scratchpad");
        initial.content = defaultScratchpadContent();
        m_sheets.push_back(initial);
        m_activeIndex = 0;
        saveSheet(0);
        saveIndex();
    } else {
        setActiveIndex(m_activeIndex);
    }
}

bool SheetStore::saveIndex(QString *errorOut) const {
    QJsonArray arr;
    for (const Sheet &s : m_sheets) {
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), s.id);
        obj.insert(QStringLiteral("title"), s.title);
        if (!s.externalPath.isEmpty())
            obj.insert(QStringLiteral("externalPath"), s.externalPath);
        arr.append(obj);
    }
    QJsonObject root;
    root.insert(QStringLiteral("activeIndex"), m_activeIndex);
    root.insert(QStringLiteral("precision"), m_precision);
    root.insert(QStringLiteral("sheets"), arr);

    return writeAtomically(m_dataDir + QStringLiteral("/index.json"),
                           QJsonDocument(root).toJson(QJsonDocument::Indented), errorOut);
}

bool SheetStore::saveSheet(int index, QString *errorOut) {
    if (index < 0 || index >= m_sheets.size())
        return false;

    Sheet &s = m_sheets[index];
    const QString target = s.externalPath.isEmpty() ? sheetFilePath(s.id) : s.externalPath;
    if (!writeAtomically(target, s.content.toUtf8(), errorOut))
        return false;
    s.dirty = false;
    return true;
}

bool SheetStore::saveAllManaged(QString *errorOut) {
    for (int i = 0; i < m_sheets.size(); ++i) {
        if (m_sheets[i].externalPath.isEmpty() && m_sheets[i].dirty) {
            if (!saveSheet(i, errorOut))
                return false;
        }
    }
    return saveIndex(errorOut);
}

int SheetStore::addSheet(const QString &title, const QString &content, const QString &externalPath) {
    Sheet s;
    s.id = nextSheetId();
    s.title = title.isEmpty() ? nextDefaultTitle() : title;
    s.content = normalizeLineEndings(content);
    s.externalPath = externalPath;
    s.dirty = false;
    m_sheets.push_back(s);
    const int idx = m_sheets.size() - 1;
    m_activeIndex = idx;
    if (s.externalPath.isEmpty())
        saveSheet(idx);
    saveIndex();
    return idx;
}

int SheetStore::duplicateSheet(int index) {
    if (index < 0 || index >= m_sheets.size())
        return -1;
    const Sheet &src = m_sheets.at(index);
    const QString baseTitle = src.title;
    QString candidate = baseTitle + QStringLiteral(" (copy)");
    for (int n = 2;; ++n) {
        bool clash = false;
        for (const Sheet &s : m_sheets) {
            if (s.title == candidate) {
                clash = true;
                break;
            }
        }
        if (!clash)
            break;
        candidate = QStringLiteral("%1 (copy %2)").arg(baseTitle).arg(n);
    }
    return addSheet(candidate, src.content);
}

bool SheetStore::renameSheet(int index, const QString &title) {
    if (index < 0 || index >= m_sheets.size())
        return false;
    const QString clean = title.trimmed();
    if (clean.isEmpty())
        return false;
    m_sheets[index].title = clean;
    saveIndex();
    return true;
}

bool SheetStore::closeSheet(int index) {
    if (index < 0 || index >= m_sheets.size())
        return false;

    const Sheet removed = m_sheets.takeAt(index);
    if (removed.externalPath.isEmpty())
        QFile::remove(sheetFilePath(removed.id));

    if (m_sheets.isEmpty()) {
        addSheet(QStringLiteral("Sheet 1"), {});
        return true;
    }

    if (m_activeIndex >= m_sheets.size())
        m_activeIndex = m_sheets.size() - 1;
    else if (m_activeIndex > index)
        --m_activeIndex;

    saveIndex();
    return true;
}

void SheetStore::setSheetContent(int index, const QString &content) {
    if (index < 0 || index >= m_sheets.size())
        return;
    const QString normalized = normalizeLineEndings(content);
    if (m_sheets[index].content == normalized)
        return;
    m_sheets[index].content = normalized;
    m_sheets[index].dirty = true;
}

int SheetStore::openExternalFile(const QString &path, QString *errorOut) {
    const QString canonical = QFileInfo(path).absoluteFilePath();
    for (int i = 0; i < m_sheets.size(); ++i) {
        if (!m_sheets[i].externalPath.isEmpty() &&
            QFileInfo(m_sheets[i].externalPath).absoluteFilePath() == canonical) {
            m_activeIndex = i;
            saveIndex();
            return i;
        }
    }

    QFile file(canonical);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorOut)
            *errorOut = file.errorString();
        return -1;
    }
    const QString content = normalizeLineEndings(QString::fromUtf8(file.readAll()));
    const QString title = QFileInfo(canonical).fileName();
    return addSheet(title, content, canonical);
}

bool SheetStore::saveSheetToExternalFile(int index, const QString &path, QString *errorOut) {
    if (index < 0 || index >= m_sheets.size())
        return false;
    const QString canonical = QFileInfo(path).absoluteFilePath();
    const QString oldId = m_sheets[index].id;
    const bool wasManaged = m_sheets[index].externalPath.isEmpty();

    if (!writeAtomically(canonical, m_sheets[index].content.toUtf8(), errorOut))
        return false;

    m_sheets[index].externalPath = canonical;
    m_sheets[index].title = QFileInfo(canonical).fileName();
    m_sheets[index].dirty = false;
    if (wasManaged)
        QFile::remove(sheetFilePath(oldId));
    saveIndex();
    return true;
}

bool SheetStore::hasUnsavedExternalSheets() const {
    for (const Sheet &s : m_sheets) {
        if (!s.externalPath.isEmpty() && s.dirty)
            return true;
    }
    return false;
}
