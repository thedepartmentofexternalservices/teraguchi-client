#pragma once
#include "macinputaccess.h"
#include <QObject>
#include <QUrl>
#include <functional>

class MacInputPermissions : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool checked READ checked NOTIFY statusChanged)
    Q_PROPERTY(bool supported READ supported NOTIFY statusChanged)
    Q_PROPERTY(bool accessibility READ accessibility NOTIFY statusChanged)
    Q_PROPERTY(bool inputMonitoring READ inputMonitoring NOTIFY statusChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY statusChanged)
    Q_PROPERTY(QString applicationName READ applicationName CONSTANT)
public:
    explicit MacInputPermissions(QObject* parent = nullptr);
    // Native test seams; neither capability is configurable from QML.
    using Probe = std::function<MacInputAccess::Status()>;
    using Opener = std::function<bool(const QUrl&)>;
    enum class Permission { Accessibility, InputMonitoring };
    using Requester = std::function<void(Permission)>;
    MacInputPermissions(Probe probe, Opener opener, QObject* parent = nullptr);
    MacInputPermissions(Probe probe, Opener opener, Requester requester, QObject* parent = nullptr);
    bool checked() const { return m_Checked; }
    bool supported() const { return m_Status.supported; }
    bool accessibility() const { return m_Status.accessibility; }
    bool inputMonitoring() const { return m_Status.inputMonitoring; }
    bool ready() const { return m_Checked && m_Status.ready(); }
    QString applicationName() const;
    Q_INVOKABLE bool refresh();
    // Called only by explicit user actions. Opening Settings never grants access.
    Q_INVOKABLE bool openAccessibilitySettings();
    Q_INVOKABLE bool openInputMonitoringSettings();
    // Requests the named OS permission only after an explicit user click.
    // A successful dispatch does not mean access was granted.
    Q_INVOKABLE bool requestAccessibility();
    Q_INVOKABLE bool requestInputMonitoring();
signals:
    void statusChanged();
private:
    bool openPanel(const char* panel);
    bool requestPermission(Permission permission);
    Probe m_Probe;
    Opener m_Opener;
    Requester m_Requester;
    bool m_Checked = false;
    MacInputAccess::Status m_Status;
};
