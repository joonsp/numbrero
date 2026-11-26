#pragma once

#include <QString>
#include <QVector>

struct Sheet {
    QString id;
    QString title;
    QString content;
    QString externalPath;
    bool dirty = false;
};

class SheetStore {
public:
    explicit SheetStore(const QString &dataDir = {});

    QString dataDir() const { return m_dataDir; }
    const QVector<Sheet> &sheets() const { return m_sheets; }
    int count() const { return m_sheets.size(); }
    int activeIndex() const { return m_activeIndex; }
    void setActiveIndex(int index);
    int precision() const { return m_precision; }
    void setPrecision(int precision);

    void load();
    bool saveIndex(QString *errorOut = nullptr) const;
    bool saveSheet(int index, QString *errorOut = nullptr);
    bool saveAllManaged(QString *errorOut = nullptr);

    int addSheet(const QString &title = {}, const QString &content = {}, const QString &externalPath = {});
    int duplicateSheet(int index);
    bool renameSheet(int index, const QString &title);
    bool closeSheet(int index);
    void setSheetContent(int index, const QString &content);

    int openExternalFile(const QString &path, QString *errorOut = nullptr);
    bool saveSheetToExternalFile(int index, const QString &path, QString *errorOut = nullptr);
    bool hasUnsavedExternalSheets() const;

    static QString normalizeLineEndings(QString text);
    static QString defaultScratchpadContent();

private:
    QString sheetFilePath(const QString &id) const;
    QString nextSheetId() const;
    QString nextDefaultTitle() const;
    static bool writeAtomically(const QString &path, const QByteArray &data, QString *errorOut);

    QString m_dataDir;
    QVector<Sheet> m_sheets;
    int m_activeIndex = 0;
    int m_precision = 6;
};
