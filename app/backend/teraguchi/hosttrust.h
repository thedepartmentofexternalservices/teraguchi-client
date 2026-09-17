#pragma once
#include "studiosetup.h"
#include <QHostAddress>
#include <QUrl>

namespace TeraguchiStudio {
// Native-only, immutable admission snapshot. Never persisted with a bookmark.
struct HostTrust {
    Lease setup;
    QString nodeId, hostId, address;
    quint16 port = 0;
    bool valid() const {
        if (!setup || setup->development || !setup->valid() || !port ||
                QHostAddress(address).isNull()) return false;
        const auto entry = setup->profile.workstations.constFind(nodeId);
        return entry != setup->profile.workstations.cend() && entry->hostId == hostId &&
                !entry->certificates.isEmpty();
    }
    bool permits(const QUrl& url) const {
        return valid() && url.scheme() == QLatin1String("https") && url.userInfo().isEmpty() &&
                url.fragment().isEmpty() && url.port(0) == port &&
                !QHostAddress(url.host()).isNull() && QHostAddress(url.host()) == QHostAddress(address);
    }
    bool accepts(const QByteArray& fingerprint) const {
        return valid() && fingerprint.size() == 32 &&
                setup->profile.workstations.value(nodeId).certificates.contains(fingerprint);
    }
};
using HostLease = std::shared_ptr<const HostTrust>;
}
