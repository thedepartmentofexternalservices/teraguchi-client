#pragma once
#include <QHostAddress>
#include <QVariantMap>

// The login handle binds one account, Tailscale node, route and PLANK identity.
// It must be resolved again before sending credentials or creating a Session.
namespace TeraguchiAssignment {
inline bool matches(const QVariantMap& expected, const QVariantMap& current)
{
    for (const auto& key : {"id", "identity", "computerId", "hostId", "address"}) {
        if (expected.value(QLatin1String(key)).toString().isEmpty() ||
                expected.value(QLatin1String(key)) != current.value(QLatin1String(key))) return false;
    }
    return current.value(QStringLiteral("status")).toString() == QStringLiteral("ready");
}
}
