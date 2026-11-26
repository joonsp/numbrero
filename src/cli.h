#pragma once

#include <QString>
#include <QStringList>

class Cli {
public:
    // Returns >= 0 if a CLI command ran and the process should exit with that code,
    // or -1 if the GUI should launch (with any file paths stored in guiFilesOut).
    static int run(const QStringList &args, QStringList &guiFilesOut, const QString &dataDir = {});
};
