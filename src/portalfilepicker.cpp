// Portal file picker pattern adapted from Omacut (MIT, David Heinemeier Hansson).
#include "portalfilepicker.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QUrl>

namespace {

constexpr auto kPortalService = "org.freedesktop.portal.Desktop";
constexpr auto kPortalPath = "/org/freedesktop/portal/desktop";
constexpr auto kChooserInterface = "org.freedesktop.portal.FileChooser";
constexpr auto kRequestInterface = "org.freedesktop.portal.Request";

class PortalResponseReceiver : public QObject {
    Q_OBJECT

public:
    PortalResponseReceiver(const QString &requestPath, bool isSave, PortalFilePicker *picker)
        : QObject(picker), m_requestPath(requestPath), m_isSave(isSave), m_picker(picker) {
        QDBusConnection::sessionBus().connect(
            kPortalService, m_requestPath, kRequestInterface, "Response",
            this, SLOT(onResponse(uint,QVariantMap)));
    }

public slots:
    void onResponse(uint response, const QVariantMap &results) {
        QDBusConnection::sessionBus().disconnect(
            kPortalService, m_requestPath, kRequestInterface, "Response",
            this, SLOT(onResponse(uint,QVariantMap)));
        deleteLater();

        if (response != 0)
            return;
        const QStringList uris = results.value("uris").toStringList();
        if (uris.isEmpty())
            return;
        const QUrl url(uris.first());
        const QString localPath = url.isLocalFile() ? url.toLocalFile() : url.toString();
        if (localPath.isEmpty())
            return;

        if (m_isSave)
            emit m_picker->savePicked(localPath);
        else
            emit m_picker->filePicked(localPath);
    }

private:
    QString m_requestPath;
    bool m_isSave;
    PortalFilePicker *m_picker;
};

} // namespace

PortalFilePicker::PortalFilePicker(QObject *parent) : FilePicker(parent) {}

void PortalFilePicker::openFile(const QString &title) {
    invokeChooser(QStringLiteral("OpenFile"), title, {}, false);
}

void PortalFilePicker::saveFile(const QString &title, const QString &suggestedName) {
    QVariantMap extra;
    if (!suggestedName.isEmpty())
        extra.insert(QStringLiteral("current_name"), suggestedName);
    invokeChooser(QStringLiteral("SaveFile"), title, extra, true);
}

void PortalFilePicker::invokeChooser(const QString &method, const QString &title,
                                     const QVariantMap &extraOptions, bool isSave) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        emit errorOccurred(QCoreApplication::translate("PortalFilePicker",
                                                       "Session D-Bus is unavailable"));
        return;
    }

    QString sender = bus.baseService();
    if (sender.startsWith(QLatin1Char(':')))
        sender.remove(0, 1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));

    const QString token = QStringLiteral("numbrero_%1").arg(++m_seq);
    const QString expectedPath =
        QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);

    auto *receiver = new PortalResponseReceiver(expectedPath, isSave, this);

    QVariantMap options = extraOptions;
    options.insert(QStringLiteral("handle_token"), token);

    QDBusMessage msg = QDBusMessage::createMethodCall(
        kPortalService, kPortalPath, kChooserInterface, method);
    msg << QString() << title << options;

    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, receiver, watcher](QDBusPendingCallWatcher *) {
        watcher->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply(*watcher);
        if (reply.isError()) {
            receiver->deleteLater();
            emit errorOccurred(
                QCoreApplication::translate("PortalFilePicker", "File dialog unavailable: %1")
                    .arg(reply.error().message()));
        }
    });
}

#include "portalfilepicker.moc"
