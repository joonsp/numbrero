#include "backend.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QLocale>
#include <QQuickTextDocument>
#include <QVariantMap>

#include "filepicker.h"
#include "highlighter.h"

Backend::Backend(const QString &dataDir, FilePicker *picker, QObject *parent)
    : QObject(parent), m_store(dataDir), m_picker(picker) {
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(250);
    connect(&m_saveTimer, &QTimer::timeout, this, &Backend::flushPendingSaves);

    m_noticeTimer.setSingleShot(true);
    m_noticeTimer.setInterval(2600);
    connect(&m_noticeTimer, &QTimer::timeout, this, [this] {
        if (!m_statusNotice.isEmpty()) {
            m_statusNotice.clear();
            emit statusNoticeChanged();
        }
    });

    if (m_picker) {
        connect(m_picker, &FilePicker::filePicked, this, [this](const QString &path) {
            openPath(path);
        });
        connect(m_picker, &FilePicker::savePicked, this, [this](const QString &path) {
            const int target = m_saveAsTargetIndex >= 0 ? m_saveAsTargetIndex : m_store.activeIndex();
            m_saveAsTargetIndex = -1;
            QString err;
            if (m_store.saveSheetToExternalFile(target, path, &err)) {
                emit sheetsChanged();
                emit activeSheetChanged();
                setNotice(tr("Saved %1").arg(activeTitle()));
            } else {
                setNotice(tr("Save failed: %1").arg(err));
            }
        });
        connect(m_picker, &FilePicker::errorOccurred, this, [this](const QString &msg) {
            setNotice(msg);
        });
    }

    m_store.load();
    evaluateCurrent();
}

QVariantList Backend::sheets() const {
    QVariantList list;
    const auto &all = m_store.sheets();
    for (int i = 0; i < all.size(); ++i) {
        const Sheet &s = all.at(i);
        QVariantMap item;
        item.insert(QStringLiteral("id"), s.id);
        item.insert(QStringLiteral("title"), s.title);
        item.insert(QStringLiteral("dirty"), s.dirty && !s.externalPath.isEmpty());
        item.insert(QStringLiteral("externalPath"), s.externalPath);
        item.insert(QStringLiteral("active"), i == m_store.activeIndex());
        list.push_back(item);
    }
    return list;
}

QString Backend::activeTitle() const {
    const auto &all = m_store.sheets();
    const int idx = m_store.activeIndex();
    return (idx >= 0 && idx < all.size()) ? all.at(idx).title : QString();
}

QString Backend::activeText() const {
    const auto &all = m_store.sheets();
    const int idx = m_store.activeIndex();
    return (idx >= 0 && idx < all.size()) ? all.at(idx).content : QString();
}

QString Backend::activeFilePath() const {
    const auto &all = m_store.sheets();
    const int idx = m_store.activeIndex();
    return (idx >= 0 && idx < all.size()) ? all.at(idx).externalPath : QString();
}

bool Backend::activeDirty() const {
    const auto &all = m_store.sheets();
    const int idx = m_store.activeIndex();
    return (idx >= 0 && idx < all.size()) && all.at(idx).dirty && !all.at(idx).externalPath.isEmpty();
}

void Backend::setVariablesBarVisible(bool visible) {
    if (m_variablesBarVisible == visible)
        return;
    m_variablesBarVisible = visible;
    emit variablesBarVisibleChanged();
}

void Backend::toggleVariablesBar() {
    setVariablesBarVisible(!m_variablesBarVisible);
}

void Backend::setPrecision(int precision) {
    const int before = m_store.precision();
    m_store.setPrecision(precision);
    if (m_store.precision() == before)
        return;
    evaluateCurrent();
    emit precisionChanged();
    setNotice(tr("Decimals: %1").arg(m_store.precision()));
}

void Backend::cyclePrecision() {
    const int cur = m_store.precision();
    const int next = cur < 4 ? 4 : (cur < 6 ? 6 : (cur < 8 ? 8 : 2));
    setPrecision(next);
}

void Backend::setActiveText(const QString &text) {
    const int idx = m_store.activeIndex();
    if (idx < 0 || idx >= m_store.count())
        return;
    const QString normalized = SheetStore::normalizeLineEndings(text);
    if (m_store.sheets().at(idx).content == normalized)
        return;

    const bool wasDirty = activeDirty();
    m_store.setSheetContent(idx, normalized);
    evaluateCurrent();
    emit activeTextChanged();

    if (activeFilePath().isEmpty()) {
        m_saveTimer.start();
    } else if (!wasDirty) {
        emit sheetsChanged();
        emit activeSheetChanged();
    }
}

void Backend::selectSheet(int index) {
    if (index < 0 || index >= m_store.count() || index == m_store.activeIndex())
        return;
    flushPendingSaves();
    m_store.setActiveIndex(index);
    m_store.saveIndex();
    evaluateCurrent();
    emit sheetsChanged();
    emit activeSheetChanged();
    emit activeTextChanged();
}

void Backend::newSheet() {
    flushPendingSaves();
    m_store.addSheet();
    evaluateCurrent();
    emit sheetsChanged();
    emit activeSheetChanged();
    emit activeTextChanged();
}

void Backend::duplicateSheet(int index) {
    flushPendingSaves();
    if (m_store.duplicateSheet(index) < 0)
        return;
    evaluateCurrent();
    emit sheetsChanged();
    emit activeSheetChanged();
    emit activeTextChanged();
    setNotice(tr("Duplicated %1").arg(activeTitle()));
}

void Backend::renameSheet(int index, const QString &title) {
    if (!m_store.renameSheet(index, title))
        return;
    emit sheetsChanged();
    emit activeSheetChanged();
}

void Backend::closeSheet(int index) {
    if (!m_store.closeSheet(index))
        return;
    evaluateCurrent();
    emit sheetsChanged();
    emit activeSheetChanged();
    emit activeTextChanged();
}

void Backend::openFileDialog() {
    if (!m_picker) {
        setNotice(tr("File dialog unavailable"));
        return;
    }
    m_picker->openFile(tr("Open Sheet"));
}

bool Backend::saveCurrentSheet() {
    const int idx = m_store.activeIndex();
    if (idx < 0 || idx >= m_store.count())
        return false;

    if (activeFilePath().isEmpty()) {
        saveFileAsDialog();
        return true;
    }

    QString err;
    if (m_store.saveSheet(idx, &err)) {
        emit sheetsChanged();
        emit activeSheetChanged();
        setNotice(tr("Saved %1").arg(activeTitle()));
        return true;
    }
    setNotice(tr("Save failed: %1").arg(err));
    return false;
}

void Backend::saveFileAsDialog() {
    if (!m_picker) {
        setNotice(tr("File dialog unavailable"));
        return;
    }
    m_saveAsTargetIndex = m_store.activeIndex();
    QString suggested = activeTitle();
    if (!suggested.endsWith(QStringLiteral(".txt")) && !suggested.endsWith(QStringLiteral(".numbrero")))
        suggested += QStringLiteral(".txt");
    m_picker->saveFile(tr("Save Sheet As"), suggested);
}

bool Backend::openPath(const QString &path) {
    flushPendingSaves();
    QString err;
    const int idx = m_store.openExternalFile(path, &err);
    if (idx < 0) {
        setNotice(tr("Could not open file: %1").arg(err));
        return false;
    }
    evaluateCurrent();
    emit sheetsChanged();
    emit activeSheetChanged();
    emit activeTextChanged();
    return true;
}

void Backend::copyToClipboard(const QString &text) {
    if (text.isEmpty())
        return;
    if (QClipboard *cb = QGuiApplication::clipboard())
        cb->setText(text);
    setNotice(tr("Copied %1").arg(text));
}

void Backend::copyLastResult() {
    for (int i = m_results.size() - 1; i >= 0; --i) {
        const QVariantMap row = m_results.at(i).toMap();
        const QString copyText = row.value(QStringLiteral("copyText")).toString();
        if (!copyText.isEmpty()) {
            copyToClipboard(copyText);
            return;
        }
    }
}

void Backend::copySheetWithResults() {
    const QString text = activeText();
    const SheetResult res = Engine::evaluate(text, QLocale(), m_store.precision());
    const QStringList rawLines = text.split(QLatin1Char('\n'));
    QStringList outLines;
    outLines.reserve(rawLines.size());

    for (int i = 0; i < rawLines.size(); ++i) {
        QString line = rawLines.at(i);
        if (i < res.lines.size() && !res.lines.at(i).displayText.isEmpty())
            line += QStringLiteral("  // ") + res.lines.at(i).displayText;
        outLines.push_back(line);
    }

    if (QClipboard *cb = QGuiApplication::clipboard())
        cb->setText(outLines.join(QLatin1Char('\n')));
    setNotice(tr("Copied sheet with results"));
}

void Backend::attachHighlighter(QObject *quickTextDoc, const QString &accent) {
    auto *quickDoc = qobject_cast<QQuickTextDocument *>(quickTextDoc);
    if (!quickDoc || !quickDoc->textDocument())
        return;
    if (m_highlighter)
        m_highlighter->deleteLater();
    m_highlighter = new Highlighter(quickDoc->textDocument());
    if (!accent.isEmpty())
        m_highlighter->setAccent(accent);
}

void Backend::updateHighlighterAccent(const QString &accent) {
    if (m_highlighter)
        m_highlighter->setAccent(accent);
}

void Backend::reformatForLocale() {
    evaluateCurrent();
}

void Backend::flushPendingSaves() {
    m_saveTimer.stop();
    m_store.saveAllManaged();
}

void Backend::evaluateCurrent() {
    const SheetResult res = Engine::evaluate(activeText(), QLocale(), m_store.precision());
    m_lineCount = res.lines.size();

    QVariantList nextResults;
    nextResults.reserve(res.lines.size());
    for (const LineResult &line : res.lines) {
        QVariantMap row;
        row.insert(QStringLiteral("lineNumber"), line.lineNumber);
        row.insert(QStringLiteral("charOffset"), line.charOffset);
        row.insert(QStringLiteral("kind"), kindToString(line.kind));
        row.insert(QStringLiteral("variableName"), line.variableName);
        row.insert(QStringLiteral("displayText"), line.displayText);
        row.insert(QStringLiteral("copyText"), line.copyText);
        row.insert(QStringLiteral("errorText"), line.errorText);
        nextResults.push_back(row);
    }

    QVariantList nextVars;
    nextVars.reserve(res.variables.size());
    for (const VariableEntry &var : res.variables) {
        QVariantMap row;
        row.insert(QStringLiteral("name"), var.name);
        row.insert(QStringLiteral("displayText"), var.displayText);
        row.insert(QStringLiteral("copyText"), var.copyText);
        row.insert(QStringLiteral("lineNumber"), var.lineNumber);
        nextVars.push_back(row);
    }

    m_results = nextResults;
    m_variables = nextVars;
    emit evaluationChanged();
}

void Backend::setNotice(const QString &message) {
    m_statusNotice = message;
    emit statusNoticeChanged();
    if (!message.isEmpty())
        m_noticeTimer.start();
}

QString Backend::kindToString(LineKind kind) {
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
