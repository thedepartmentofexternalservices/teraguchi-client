#include "supportdiagnostics.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <utility>
#ifdef Q_OS_UNIX
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
constexpr int MaxBytes = 4096;
QString choice(const QVariant& value, const QStringList& allowed)
{
    if (value.metaType().id() == QMetaType::QString && value.toString().size() <= 64 && allowed.contains(value.toString()))
        return value.toString();
    return QStringLiteral("unknown");
}
QString flag(const QVariant& value, const char* yes, const char* no)
{
    if (value.metaType().id() != QMetaType::Bool) return QStringLiteral("unknown");
    return QLatin1String(value.toBool() ? yes : no);
}
QString numericVersion(const QString& value)
{
    // Omit branch labels and other build/user-supplied suffixes.
    static const QRegularExpression pattern(QRegularExpression::anchoredPattern(
        QStringLiteral("([0-9]{1,3}\\.[0-9]{1,3}(?:\\.[0-9]{1,3})?)(?:-[a-zA-Z0-9-]+)?")));
    if (value.size() > 128) return QStringLiteral("unknown");
    const auto match = pattern.match(value);
    return match.hasMatch() ? match.captured(1) : QStringLiteral("unknown");
}
bool safePath(const QString& path)
{
    if (!QDir::isAbsolutePath(path) || QDir::cleanPath(path) != path) return false;
    // Walk existing parents too: linked worktrees have a .git file, not directory.
    for (QString current = path; ; current = QFileInfo(current).absolutePath()) {
        if (QFileInfo(current).isSymLink() || QFileInfo::exists(current + QStringLiteral("/.git"))) return false;
        const auto parent = QFileInfo(current).absolutePath();
        if (parent == current) return true;
    }
}
bool writeReport(const QString& directory, const QByteArray& report)
{
#ifdef Q_OS_UNIX
    if (report.isEmpty() || report.size() > MaxBytes || !safePath(directory) ||
            !QDir().mkpath(QFileInfo(directory).absolutePath())) return false;
    const auto path = QFile::encodeName(directory);
    if (::mkdir(path.constData(), 0700) != 0 && errno != EEXIST) return false;
    const int dir = ::open(path.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (dir < 0) return false;
    struct stat info {};
    if (::fstat(dir, &info) != 0 || info.st_uid != ::geteuid() || (info.st_mode & 0077) != 0) {
        ::close(dir); return false;
    }
    int file = -1;
    QByteArray name;
    for (int attempt = 0; attempt < 3 && file < 0; ++attempt) {
        name = "support-" + QByteArray::number(QRandomGenerator::global()->generate64(), 16) + ".json";
        file = ::openat(dir, name.constData(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (file < 0 && errno != EEXIST) break;
    }
    bool ok = file >= 0;
    if (ok) {
        ok = ::fchmod(file, 0600) == 0;
        qsizetype offset = 0;
        while (ok && offset < report.size()) {
            const auto count = ::write(file, report.constData() + offset, report.size() - offset);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) ok = false;
            else offset += count;
        }
        if (ok) ok = ::fsync(file) == 0;
        if (::close(file) != 0) ok = false;
        if (!ok) ::unlinkat(dir, name.constData(), 0);
    }
    ::close(dir);
    return ok;
#else
    Q_UNUSED(directory); Q_UNUSED(report);
    return false;
#endif
}
}

SupportDiagnostics::SupportDiagnostics(QObject* parent)
    : SupportDiagnostics(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                         QStringLiteral("/support-reports"), QDesktopServices::openUrl, parent) {}
SupportDiagnostics::SupportDiagnostics(QString directory, std::function<bool(const QUrl&)> opener, QObject* parent)
    : QObject(parent), m_Directory(std::move(directory)), m_Opener(std::move(opener)) {}

QByteArray SupportDiagnostics::serialize(const QVariantMap& state, const QString& version, const QString& osVersion)
{
    QJsonObject status;
    status.insert("studio_setup", choice(state.value("studio_setup"), {"unconfigured", "needed", "ready", "development", "expired", "invalid"}));
    status.insert("tailscale", choice(state.value("tailscale"), {"configuration-needed", "missing", "needs-login", "needs-approval", "stopped", "no-shared-workstations", "ready", "refreshing", "unavailable"}));
    status.insert("phase", choice(state.value("phase"), {"idle", "checking", "connecting", "connected", "interrupted", "blocked"}));
    status.insert("issue", choice(state.value("issue"), {"none", "assignment", "permissions", "displays", "tablet", "trust", "seat", "workstation", "login", "video", "connection"}));
    status.insert("catalog", flag(state.value("catalog_fresh"), "fresh", "stale"));
    status.insert("catalog_refresh", flag(state.value("catalog_refreshing"), "pending", "idle"));
    status.insert("session_resources", flag(state.value("runtime_pending"), "retained", "released"));
    status.insert("workstation", choice(state.value("workstation"), {"none", "ready", "offline", "occupied", "incompatible", "unknown"}));
    const bool checked = state.value("permissions_checked").metaType().id() == QMetaType::Bool && state.value("permissions_checked").toBool();
    const auto supported = state.value("permissions_supported");
    for (const auto* name : {"accessibility", "input_monitoring"}) {
        status.insert(QLatin1String(name), !checked ? QStringLiteral("unknown") :
            supported.metaType().id() != QMetaType::Bool ? QStringLiteral("unknown") :
            !supported.toBool() ? QStringLiteral("unsupported") : flag(state.value(QLatin1String(name)), "allowed", "needed"));
    }
    const auto displays = state.value("selected_displays");
    const bool numeric = displays.metaType().id() == QMetaType::Int || displays.metaType().id() == QMetaType::Double;
    status.insert("selected_displays", numeric && (displays.toDouble() == 1 || displays.toDouble() == 2) ?
                      QJsonValue(displays.toInt()) : QJsonValue(QJsonValue::Null));
    status.insert("tablet_path", "not-tested");
    status.insert("physical_displays", "not-tested");
    status.insert("video_path", "not-tested");
    QJsonObject root {{"schema_version", 1}, {"scope", "launcher-status-only"},
                      {"client_version", numericVersion(version)}, {"qt_version", numericVersion(QString::fromLatin1(qVersion()))},
                      {"os_version", numericVersion(osVersion)}, {"status", status}};
#if defined(Q_OS_MACOS) && defined(Q_PROCESSOR_ARM_64)
    root.insert("platform", "macos-arm64");
#else
    root.insert("platform", "other");
#endif
    const auto bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    return bytes.size() <= MaxBytes ? bytes : QByteArray();
}
void SupportDiagnostics::prepare(const QVariantMap& state)
{
    m_Report = serialize(state, QCoreApplication::applicationVersion(), QSysInfo::productVersion());
    m_Saved = false;
    m_Message.clear();
    emit changed();
}
bool SupportDiagnostics::save()
{
    if (m_Saved) return false;
    m_Saved = writeReport(m_Directory, m_Report);
    m_Message = m_Saved ? tr("Saved a private support report. Use Show folder to find it.") :
        tr("Couldn't save the report. Check that the client's application data folder is writable and private, then try again.");
    emit changed();
    return m_Saved;
}
bool SupportDiagnostics::showFolder()
{
    if (!m_Saved) return false;
    const bool opened = safePath(m_Directory) && m_Opener(QUrl::fromLocalFile(m_Directory));
    if (!opened) { m_Message = tr("Couldn't open the report folder. Your saved report is still in the client's application data folder."); emit changed(); }
    return opened;
}
