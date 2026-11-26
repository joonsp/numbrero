#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>

#include "engine.h"
#include "sheetstore.h"

class FilePicker;
class Highlighter;

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList sheets READ sheets NOTIFY sheetsChanged)
    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeSheetChanged)
    Q_PROPERTY(QString activeTitle READ activeTitle NOTIFY activeSheetChanged)
    Q_PROPERTY(QString activeText READ activeText NOTIFY activeTextChanged)
    Q_PROPERTY(QString activeFilePath READ activeFilePath NOTIFY activeSheetChanged)
    Q_PROPERTY(bool activeDirty READ activeDirty NOTIFY activeSheetChanged)
    Q_PROPERTY(bool hasUnsavedWork READ hasUnsavedWork NOTIFY sheetsChanged)
    Q_PROPERTY(QVariantList results READ results NOTIFY evaluationChanged)
    Q_PROPERTY(QVariantList variables READ variables NOTIFY evaluationChanged)
    Q_PROPERTY(int lineCount READ lineCount NOTIFY evaluationChanged)
    Q_PROPERTY(int variableCount READ variableCount NOTIFY evaluationChanged)
    Q_PROPERTY(QString statusNotice READ statusNotice NOTIFY statusNoticeChanged)
    Q_PROPERTY(bool variablesBarVisible READ variablesBarVisible WRITE setVariablesBarVisible NOTIFY variablesBarVisibleChanged)
    Q_PROPERTY(int precision READ precision WRITE setPrecision NOTIFY precisionChanged)

public:
    explicit Backend(const QString &dataDir = {}, FilePicker *picker = nullptr, QObject *parent = nullptr);

    QVariantList sheets() const;
    int activeIndex() const { return m_store.activeIndex(); }
    QString activeTitle() const;
    QString activeText() const;
    QString activeFilePath() const;
    bool activeDirty() const;
    bool hasUnsavedWork() const { return m_store.hasUnsavedExternalSheets(); }
    QVariantList results() const { return m_results; }
    QVariantList variables() const { return m_variables; }
    int lineCount() const { return m_lineCount; }
    int variableCount() const { return m_variables.size(); }
    QString statusNotice() const { return m_statusNotice; }
    bool variablesBarVisible() const { return m_variablesBarVisible; }
    void setVariablesBarVisible(bool visible);
    int precision() const { return m_store.precision(); }
    void setPrecision(int precision);

    Q_INVOKABLE void setActiveText(const QString &text);
    Q_INVOKABLE void selectSheet(int index);
    Q_INVOKABLE void newSheet();
    Q_INVOKABLE void duplicateSheet(int index);
    Q_INVOKABLE void renameSheet(int index, const QString &title);
    Q_INVOKABLE void closeSheet(int index);

    Q_INVOKABLE void openFileDialog();
    Q_INVOKABLE bool saveCurrentSheet();
    Q_INVOKABLE void saveFileAsDialog();
    Q_INVOKABLE bool openPath(const QString &path);
    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE void copyLastResult();
    Q_INVOKABLE void copySheetWithResults();
    Q_INVOKABLE void toggleVariablesBar();
    Q_INVOKABLE void cyclePrecision();

    Q_INVOKABLE void attachHighlighter(QObject *quickTextDoc, const QString &accent);
    Q_INVOKABLE void updateHighlighterAccent(const QString &accent);

public slots:
    void reformatForLocale();
    void flushPendingSaves();

signals:
    void sheetsChanged();
    void activeSheetChanged();
    void activeTextChanged();
    void evaluationChanged();
    void statusNoticeChanged();
    void variablesBarVisibleChanged();
    void precisionChanged();

private:
    void evaluateCurrent();
    void setNotice(const QString &message);
    static QString kindToString(LineKind kind);

    SheetStore m_store;
    FilePicker *m_picker = nullptr;
    QPointer<Highlighter> m_highlighter;
    QTimer m_saveTimer;
    QTimer m_noticeTimer;
    QVariantList m_results;
    QVariantList m_variables;
    int m_lineCount = 0;
    QString m_statusNotice;
    bool m_variablesBarVisible = true;
    int m_saveAsTargetIndex = -1;
};
