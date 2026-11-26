// Portal file picker pattern adapted from Omacut (MIT, David Heinemeier Hansson).
#pragma once

#include <QVariantMap>

#include "filepicker.h"

class PortalFilePicker : public FilePicker {
    Q_OBJECT

public:
    explicit PortalFilePicker(QObject *parent = nullptr);

    void openFile(const QString &title) override;
    void saveFile(const QString &title, const QString &suggestedName) override;

private:
    void invokeChooser(const QString &method, const QString &title,
                       const QVariantMap &extraOptions, bool isSave);

    unsigned int m_seq = 0;
};
