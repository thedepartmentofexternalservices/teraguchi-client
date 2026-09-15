#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <QVariantList>

// Reads the local user's view only. No admin API, invitation or login mutations.
class TailscaleWorkstations : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString studioDnsSuffix READ studioDnsSuffix WRITE setStudioDnsSuffix NOTIFY configurationChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool fresh READ fresh NOTIFY stateChanged)
    Q_PROPERTY(QVariantList workstations READ workstations NOTIFY stateChanged)
public:
    struct Snapshot {
        QString state;
        QString identity;
        QVariantList workstations;
        bool valid = false;
    };
    explicit TailscaleWorkstations(QObject* parent = nullptr);
    // Native test seam; never configurable from QML or a status response.
    TailscaleWorkstations(QString executable, QStringList arguments, QObject* parent);
    ~TailscaleWorkstations() override;
    static Snapshot parseStatus(const QByteArray& json, const QString& studioDnsSuffix);
    static QString normalizedSuffix(const QString& suffix);
    QString studioDnsSuffix() const { return m_StudioDnsSuffix; }
    void setStudioDnsSuffix(const QString& suffix);
    QString state() const { return m_State; }
    bool busy() const { return !m_Process.isNull(); }
    bool fresh() const;
    QString currentIdentity() const { return m_Identity; }
    int remainingValidityMs() const;
    QVariantList workstations() const { return m_Workstations; }
    Q_INVOKABLE void refresh(int token);
    Q_INVOKABLE void cancel(int token);
    Q_INVOKABLE QVariantMap resolve(const QString& nodeId) const;

signals:
    void configurationChanged();
    void stateChanged();
    void catalogReady(int token, QVariantList entries, int validityMs);
    void catalogFailed(int token, QString reason);
    void catalogInvalidated();
    void identityChanged();

private:
    void stopProcess();
    void fail(const QString& state);
    void finish();
    QString m_Executable;
    QStringList m_Arguments;
    QString m_StudioDnsSuffix;
    QString m_State = QStringLiteral("configuration-needed");
    QString m_Identity;
    QVariantList m_Workstations;
    QPointer<QProcess> m_Process;
    QTimer m_Timeout;
    QTimer m_Expiry;
    QElapsedTimer m_RequestAge;
    QElapsedTimer m_SnapshotAge;
    qint64 m_SnapshotWallTime = 0;
    QByteArray m_Output;
    int m_Token = 0;
    bool m_Fresh = false;
};
