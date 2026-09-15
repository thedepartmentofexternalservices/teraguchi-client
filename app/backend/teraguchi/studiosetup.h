#pragma once
#include <QObject>
#include <QByteArray>
#include <QDateTime>
#include <QTimer>
#include <QUrl>
#include <QMap>
#include <QList>
#include <chrono>
#include <functional>
#include <memory>

namespace TeraguchiStudio {
struct Workstation {
    QString hostId;
    QList<QByteArray> certificates;
};
struct Profile {
    QString label, suffix;
    qint64 revision = 0, issued = 0, expires = 0;
    QByteArray digest;
    QMap<QString, Workstation> workstations;
};
struct Permit {
    Profile profile;
    qint64 admittedAt = 0;
    std::chrono::steady_clock::time_point admittedClock = std::chrono::steady_clock::now();
    bool development = false;
    bool valid(qint64 now, std::chrono::steady_clock::time_point clock) const {
        if (development) return true;
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(clock - admittedClock).count();
        return admittedAt > 0 && now >= admittedAt && now < profile.expires && elapsed >= 0 && elapsed < profile.expires - admittedAt;
    }
    bool valid() const { return valid(QDateTime::currentSecsSinceEpoch(), std::chrono::steady_clock::now()); }
};
using Lease = std::shared_ptr<const Permit>;
QByteArray pinnedPublicKey();
Profile verify(const QByteArray& envelope, const QByteArray& publicKey, qint64 now, QString* error, bool allowInactive = false);
}

// Only signed setup can enter this service. No QML trust flag, shell command,
// URL, invitation, credential or endpoint list can supply configuration.
class StudioSetup : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY statusChanged)
    Q_PROPERTY(QString label READ label NOTIFY statusChanged)
    Q_PROPERTY(QString message READ message NOTIFY statusChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY statusChanged)
    Q_PROPERTY(bool canImport READ canImport CONSTANT)
public:
    explicit StudioSetup(QObject* parent = nullptr);
    // Native fixture seam; not exposed to QML or configuration files.
    StudioSetup(QByteArray key, QString storage, std::function<qint64()> now, QObject* parent = nullptr);
    QString state() const { return m_State; }
    QString label() const { return ready() ? m_Permit->profile.label : QString(); }
    QString message() const { return m_Message; }
    bool ready() const;
    bool canImport() const { return m_Key.size() == 32; }
    TeraguchiStudio::Lease permit() const { return ready() ? m_Permit : nullptr; }
    QString suffix() const { return ready() ? m_Permit->profile.suffix : QString(); }
    void setDevelopmentSuffix(const QString& suffix);
    Q_INVOKABLE bool importFile(const QUrl& url);
    Q_INVOKABLE void refresh();
signals:
    void statusChanged();
    void configurationChanged();
private:
    void load();
    void accept(const TeraguchiStudio::Profile& profile);
    void error(const QString& message);
    QByteArray m_Key;
    QString m_Storage, m_State, m_Message;
    std::function<qint64()> m_Now;
    TeraguchiStudio::Lease m_Permit;
    TeraguchiStudio::Profile m_Highest;
    QTimer m_Timer;
};
