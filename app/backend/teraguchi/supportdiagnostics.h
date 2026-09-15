#pragma once
#include <QObject>
#include <QUrl>
#include <QVariantMap>
#include <functional>

// Deliberately has no access to logs, credentials, endpoints or input samples.
// Reports describe launcher state; they cannot establish hardware qualification.
class SupportDiagnostics : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString preview READ preview NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(bool saved READ saved NOTIFY changed)
public:
    explicit SupportDiagnostics(QObject* parent = nullptr);
    // Native fixture seam only. QML cannot choose a path or launch an arbitrary URL.
    SupportDiagnostics(QString directory, std::function<bool(const QUrl&)> opener, QObject* parent = nullptr);
    static QByteArray serialize(const QVariantMap& state, const QString& version, const QString& osVersion);
    QString preview() const { return QString::fromUtf8(m_Report); }
    QString message() const { return m_Message; }
    bool saved() const { return m_Saved; }
    Q_INVOKABLE void prepare(const QVariantMap& state);
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool showFolder();
signals:
    void changed();
private:
    QString m_Directory, m_Message;
    QByteArray m_Report;
    std::function<bool(const QUrl&)> m_Opener;
    bool m_Saved = false;
};
