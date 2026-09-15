#include "macinputpermissions.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <utility>

MacInputPermissions::MacInputPermissions(QObject* parent)
    : MacInputPermissions(MacInputAccess::query, QDesktopServices::openUrl, parent) {}
MacInputPermissions::MacInputPermissions(Probe probe, Opener opener, QObject* parent)
    : QObject(parent), m_Probe(std::move(probe)), m_Opener(std::move(opener)) {}
bool MacInputPermissions::refresh()
{
    const auto next = m_Probe();
    const bool changed = !m_Checked || next.supported != m_Status.supported ||
        next.accessibility != m_Status.accessibility || next.inputMonitoring != m_Status.inputMonitoring;
    m_Status = next;
    m_Checked = true;
    if (changed) emit statusChanged();
    return ready();
}
QString MacInputPermissions::applicationName() const
{
#ifdef __APPLE__
    if (auto bundle = CFBundleGetMainBundle()) {
        auto value = CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("CFBundleDisplayName"));
        if (value && CFGetTypeID(value) == CFStringGetTypeID())
            return QString::fromCFString(static_cast<CFStringRef>(value));
    }
#endif
    return QCoreApplication::applicationName();
}
bool MacInputPermissions::openPanel(const char* panel)
{
    if (!m_Checked || !m_Status.supported) return false;
    return m_Opener(QUrl(QStringLiteral("x-apple.systempreferences:com.apple.preference.security?") + QLatin1String(panel)));
}
bool MacInputPermissions::openAccessibilitySettings() { return openPanel("Privacy_Accessibility"); }
bool MacInputPermissions::openInputMonitoringSettings() { return openPanel("Privacy_ListenEvent"); }
