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
    MacInputPermissions(Probe probe, Opener opener, QObject* parent = nullptr);
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
signals:
    void statusChanged();
private:
    bool openPanel(const char* panel);
    Probe m_Probe;
    Opener m_Opener;
    bool m_Checked = false;
    MacInputAccess::Status m_Status;
};
