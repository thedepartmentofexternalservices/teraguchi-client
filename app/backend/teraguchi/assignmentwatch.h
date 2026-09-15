#pragma once
#include "tailscaleworkstations.h"
#include <QThread>
#include <atomic>
#include <chrono>

// Owns its event loop: Qt's UI loop is suspended by SDL on macOS.
// A failed read prevents new connections; only confirmed removal/identity
// change stops an established stream. Network access remains Tailscale's job.
class AssignmentWatch : public QThread
{
    Q_OBJECT
public:
    AssignmentWatch(QString suffix, QVariantMap target, int validityMs,
                    QString executable = {}, QStringList arguments = {}, TeraguchiStudio::Lease permit = {});
    ~AssignmentWatch() override;
    bool permitsConnection() const;
    static bool sameAssignment(const QVariantMap& expected, const QString& identity,
                               const QVariantList& entries);
signals:
    void assignmentRemoved();
protected:
    void run() override;
private:
    static qint64 now();
    TeraguchiStudio::Lease m_StudioPermit;
    QString m_Suffix;
    QString m_Executable;
    QStringList m_Arguments;
    QVariantMap m_Target;
    std::atomic<qint64> m_ExpiresAt{0};
};
