#include "assignmentwatch.h"
#include <QTimer>
#include <memory>

AssignmentWatch::AssignmentWatch(QString suffix, QVariantMap target, int validityMs,
                                 QString executable, QStringList arguments)
    : m_Suffix(std::move(suffix)), m_Executable(std::move(executable)),
      m_Arguments(std::move(arguments)), m_Target(std::move(target))
{
    m_ExpiresAt.store(now() + qBound(0, validityMs, 30000));
}
AssignmentWatch::~AssignmentWatch() { requestInterruption(); quit(); wait(); }
qint64 AssignmentWatch::now()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
bool AssignmentWatch::permitsConnection() const { return now() < m_ExpiresAt.load(); }
bool AssignmentWatch::sameAssignment(const QVariantMap& expected, const QString& identity,
                                    const QVariantList& entries)
{
    if (identity.isEmpty() || identity != expected.value("identity").toString() ||
            expected.value("id").toString().isEmpty() || expected.value("address").toString().isEmpty()) return false;
    for (const auto& value : entries) {
        const auto entry = value.toMap();
        if (entry.value("id") == expected.value("id"))
            return entry.value("address") == expected.value("address");
    }
    return false;
}
void AssignmentWatch::run()
{
    auto ownedReader = m_Executable.isEmpty() ? std::make_unique<TailscaleWorkstations>() :
        std::make_unique<TailscaleWorkstations>(m_Executable, m_Arguments, nullptr);
    auto& reader = *ownedReader;
    reader.setStudioDnsSuffix(m_Suffix);
    QTimer refresh;
    refresh.setInterval(10000);
    int token = 0;
    connect(&refresh, &QTimer::timeout, &reader, [&] {
        if (!reader.busy()) reader.refresh(++token);
    });
    connect(&reader, &TailscaleWorkstations::catalogReady, &reader,
            [&](int, const QVariantList& entries, int validityMs) {
        if (!sameAssignment(m_Target, reader.currentIdentity(), entries)) {
            m_ExpiresAt.store(0);
            emit assignmentRemoved();
            refresh.stop();
        } else {
            m_ExpiresAt.store(reader.resolve(m_Target.value("id").toString()).isEmpty() ? 0 : now() + validityMs);
        }
    });
    connect(&reader, &TailscaleWorkstations::catalogFailed, &reader, [&](int, const QString& reason) {
        m_ExpiresAt.store(0);
        if (reason == "needs-login" || reason == "needs-approval") {
            emit assignmentRemoved();
            refresh.stop();
        }
    });
    refresh.start();
    QTimer::singleShot(0, &reader, [&] {
        if (isInterruptionRequested()) QThread::currentThread()->quit();
        else reader.refresh(++token);
    });
    exec();
}
