#pragma once

#include <QObject>
#include <QString>

class FilePicker : public QObject {
    Q_OBJECT

public:
    explicit FilePicker(QObject *parent = nullptr) : QObject(parent) {}
    ~FilePicker() override = default;

    virtual void openFile(const QString &title) = 0;
    virtual void saveFile(const QString &title, const QString &suggestedName) = 0;

signals:
    void filePicked(const QString &path);
    void savePicked(const QString &path);
    void errorOccurred(const QString &message);
};
