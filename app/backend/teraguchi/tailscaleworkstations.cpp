#include "tailscaleworkstations.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <utility>

namespace {
constexpr int MaxOutput = 2 * 1024 * 1024;
constexpr int TimeoutMs = 10000;
constexpr int ValidityMs = 30000;
const QRegularExpression Label(QStringLiteral("^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$"));
const QRegularExpression StableId(QStringLiteral("^[a-zA-Z0-9_-]{1,128}$"));
QString dnsName(QString name)
{
    name = name.toLower();
    if (name.endsWith('.')) name.chop(1);
    if (name.size() > 253) return {};
    for (const auto& label : name.split('.')) {
        if (!Label.match(label).hasMatch()) return {};
    }
    return name;
}
bool nodeAddress(const QHostAddress& address)
{
    // Only node addresses. Never public endpoints, subnet routes or PeerAPI URLs.
    return address.isInSubnet(QHostAddress::parseSubnet(QStringLiteral("100.64.0.0/10"))) ||
            address.isInSubnet(QHostAddress::parseSubnet(QStringLiteral("fd7a:115c:a1e0::/48")));
}
}

QString TailscaleWorkstations::normalizedSuffix(const QString& suffix)
{
    const auto name = dnsName(suffix);
    const auto labels = name.split('.');
    // An exact tailnet DNS suffix, not all of ts.net or a wildcard.
    return labels.size() == 3 && labels[1] == QStringLiteral("ts") &&
            labels[2] == QStringLiteral("net") ? name : QString();
}

TailscaleWorkstations::Snapshot TailscaleWorkstations::parseStatus(const QByteArray& json, const QString& suffix)
{
    Snapshot result;
    result.state = QStringLiteral("unavailable");
    const auto studio = normalizedSuffix(suffix);
    if (studio.isEmpty()) { result.state = QStringLiteral("configuration-needed"); return result; }
    if (json.size() > MaxOutput) return result;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return result;
    const auto root = document.object();
    const auto backend = root.value(QStringLiteral("BackendState")).toString();
    if (backend == QStringLiteral("NeedsLogin")) { result.state = QStringLiteral("needs-login"); return result; }
    if (backend == QStringLiteral("NeedsMachineAuth")) { result.state = QStringLiteral("needs-approval"); return result; }
    if (backend == QStringLiteral("Stopped")) { result.state = QStringLiteral("stopped"); return result; }
    if (backend != QStringLiteral("Running")) return result;
    const auto self = root.value(QStringLiteral("Self")).toObject();
    const auto selfId = self.value(QStringLiteral("ID")).toString();
    const auto userId = self.value(QStringLiteral("UserID"));
    const auto tailnet = dnsName(root.value(QStringLiteral("CurrentTailnet")).toObject().value(QStringLiteral("MagicDNSSuffix")).toString());
    if (!StableId.match(selfId).hasMatch() || !userId.isDouble() || userId.toDouble() <= 0 || tailnet.isEmpty()) return result;
    if (!root.value(QStringLiteral("Peer")).isObject() && !root.value(QStringLiteral("Peer")).isNull()) return result;
    const auto peers = root.value(QStringLiteral("Peer")).toObject();
    if (peers.size() > 4096) return result;
    QSet<QString> ids, addresses, names;
    QVariantList entries;
    for (const auto& value : peers) {
        if (!value.isObject()) return result;
        const auto peer = value.toObject();
        // ShareeNode is the reverse direction: a recipient who may connect to us.
        for (const auto& field : {"ShareeNode", "Expired"}) {
            const auto flag = peer.value(QLatin1String(field));
            if (!flag.isUndefined() && !flag.isBool()) return result;
        }
        if (peer.value(QStringLiteral("ShareeNode")).toBool() ||
                peer.value(QStringLiteral("InNetworkMap")) != QJsonValue(true)) continue;
        const auto name = dnsName(peer.value(QStringLiteral("DNSName")).toString());
        const auto host = name.left(name.size() - studio.size() - 1);
        if (!name.endsWith('.' + studio) || !Label.match(host).hasMatch()) continue;
        const auto id = peer.value(QStringLiteral("ID")).toString();
        if (!StableId.match(id).hasMatch() || id == selfId || ids.contains(id) || names.contains(name)) return result;
        if (!peer.value(QStringLiteral("Online")).isBool() || !peer.value(QStringLiteral("TailscaleIPs")).isArray()) return result;
        QStringList ips;
        for (const auto& ipValue : peer.value(QStringLiteral("TailscaleIPs")).toArray()) {
            QHostAddress ip(ipValue.toString());
            if (!ipValue.isString() || !nodeAddress(ip) || !ip.scopeId().isEmpty()) return result;
            const auto address = ip.toString();
            if (addresses.contains(address) || ips.contains(address)) return result;
            ips.append(address);
        }
        if (ips.isEmpty() || ips.size() > 2) return result;
        // Prefer IPv4, with deterministic IPv6-only support. No LAN fallback.
        std::sort(ips.begin(), ips.end(), [](const QString& a, const QString& b) {
            const bool a4 = QHostAddress(a).protocol() == QAbstractSocket::IPv4Protocol;
            const bool b4 = QHostAddress(b).protocol() == QAbstractSocket::IPv4Protocol;
            return a4 != b4 ? a4 : a < b;
        });
        ids.insert(id); names.insert(name);
        for (const auto& ip : ips) addresses.insert(ip);
        const auto expiryValue = peer.value(QStringLiteral("KeyExpiry"));
        bool keyExpired = peer.value(QStringLiteral("Expired")).toBool();
        if (!expiryValue.isUndefined() && !expiryValue.isNull()) {
            const auto expiry = QDateTime::fromString(expiryValue.toString(), Qt::ISODateWithMs);
            if (!expiry.isValid()) return result;
            keyExpired |= expiry <= QDateTime::currentDateTimeUtc();
        }
        const bool online = peer.value(QStringLiteral("Online")).toBool() && !keyExpired;
        entries.append(QVariantMap{{"id", id}, {"name", host}, {"dnsName", name},
                                  {"address", ips.first()}, {"addresses", ips}, {"assigned", true},
                                  {"status", online ? "ready" : "offline"}});
    }
    std::sort(entries.begin(), entries.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap().value("id").toString() < b.toMap().value("id").toString();
    });
    // Keep identity comparison private, without retaining account profile data.
    result.identity = QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(QJsonArray{selfId, userId, tailnet}).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
    result.workstations = entries;
    result.state = entries.isEmpty() ? QStringLiteral("no-shared-workstations") : QStringLiteral("ready");
    result.valid = true;
    return result;
}

TailscaleWorkstations::TailscaleWorkstations(QObject* parent)
    : TailscaleWorkstations(QStringLiteral("/Applications/Tailscale.app/Contents/MacOS/Tailscale"),
                           {QStringLiteral("status"), QStringLiteral("--json")}, parent) { m_RequireSetup = true; }

TailscaleWorkstations::TailscaleWorkstations(QString executable, QStringList arguments, QObject* parent)
    : QObject(parent), m_Executable(std::move(executable)), m_Arguments(std::move(arguments))
{
    m_Timeout.setSingleShot(true);
    m_Timeout.setInterval(TimeoutMs);
    m_Expiry.setSingleShot(true);
    m_Expiry.setInterval(ValidityMs);
    connect(&m_Timeout, &QTimer::timeout, this, [this] { fail(QStringLiteral("unavailable")); });
    connect(&m_Expiry, &QTimer::timeout, this, [this] {
        m_Fresh = false;
        emit stateChanged();
        emit catalogInvalidated();
    });
}
TailscaleWorkstations::~TailscaleWorkstations() { stopProcess(); }

bool TailscaleWorkstations::fresh() const
{
    const auto wallAge = QDateTime::currentMSecsSinceEpoch() - m_SnapshotWallTime;
    return setupPermitsConnection() && m_Fresh && m_SnapshotAge.isValid() && m_SnapshotAge.elapsed() < ValidityMs &&
            wallAge >= 0 && wallAge < ValidityMs;
}

int TailscaleWorkstations::remainingValidityMs() const
{
    if (!fresh()) return 0;
    return static_cast<int>(ValidityMs - qMax(m_SnapshotAge.elapsed(),
        QDateTime::currentMSecsSinceEpoch() - m_SnapshotWallTime));
}

bool TailscaleWorkstations::setupPermitsConnection() const
{
    if (!m_RequireSetup) return true; // Explicit native CLI fixture constructor only.
    const auto permit = studioPermit();
    return permit && permit->valid() && permit->profile.suffix == m_StudioDnsSuffix;
}

void TailscaleWorkstations::setStudioSetup(QObject* value)
{
    if (m_Setup) disconnect(m_Setup, nullptr, this, nullptr);
    m_RequireSetup = true;
    m_Setup = qobject_cast<StudioSetup*>(value);
    const auto apply = [this] {
        // A new signed revision invalidates pending login even with the same suffix.
        stopProcess(); m_Expiry.stop(); m_Fresh = false;
        m_Workstations.clear(); m_Identity.clear();
        m_StudioDnsSuffix = m_Setup ? m_Setup->suffix() : QString();
        m_State = m_StudioDnsSuffix.isEmpty() ? QStringLiteral("configuration-needed") : QStringLiteral("unavailable");
        emit configurationChanged(); emit stateChanged(); emit catalogInvalidated(); emit identityChanged();
    };
    if (m_Setup) {
        connect(m_Setup, &StudioSetup::configurationChanged, this, apply);
        connect(m_Setup, &QObject::destroyed, this, apply);
    }
    apply();
}

void TailscaleWorkstations::setStudioDnsSuffix(const QString& suffix)
{
    const auto normalized = normalizedSuffix(suffix);
    if (normalized == m_StudioDnsSuffix) return;
    stopProcess();
    m_Expiry.stop(); m_Fresh = false;
    m_StudioDnsSuffix = normalized;
    m_Workstations.clear(); m_Identity.clear();
    m_State = normalized.isEmpty() ? QStringLiteral("configuration-needed") : QStringLiteral("unavailable");
    emit configurationChanged(); emit stateChanged();
    emit catalogInvalidated(); emit identityChanged();
}

void TailscaleWorkstations::stopProcess()
{
    m_Timeout.stop();
    if (!m_Process) return;
    auto* process = m_Process.data();
    m_Process.clear();
    disconnect(process, nullptr, this, nullptr);
    if (process->state() != QProcess::NotRunning) {
        process->kill();
        process->waitForFinished(1000);
    }
    process->deleteLater();
    m_Output.clear();
}

void TailscaleWorkstations::cancel(int token)
{
    if (!m_Process || token != m_Token) return;
    stopProcess();
    emit stateChanged();
}

void TailscaleWorkstations::refresh(int token)
{
    stopProcess();
    m_Token = token;
    m_Fresh = false; m_Expiry.stop();
    if (!setupPermitsConnection() || m_StudioDnsSuffix.isEmpty()) { fail(QStringLiteral("configuration-needed")); return; }
    if (!QFileInfo(m_Executable).isExecutable()) { fail(QStringLiteral("missing")); return; }
    m_Process = new QProcess(this);
    auto* process = m_Process.data();
    auto environment = QProcessEnvironment::systemEnvironment();
    // Tailscale's macOS app uses these to select CLI rather than GUI mode.
    environment.insert(QStringLiteral("SHLVL"), QStringLiteral("1"));
    process->setProcessEnvironment(environment);
    process->setStandardInputFile(QProcess::nullDevice());
    process->setStandardErrorFile(QProcess::nullDevice());
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process] {
        m_Output += process->readAllStandardOutput();
        if (m_Output.size() > MaxOutput) fail(QStringLiteral("unavailable"));
    });
    connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) { fail(QStringLiteral("unavailable")); });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
        if (code != 0 || status != QProcess::NormalExit) fail(QStringLiteral("unavailable"));
        else finish();
    });
    m_RequestAge.start(); m_Timeout.start();
    m_State = QStringLiteral("refreshing");
    process->start(m_Executable, m_Arguments);
    emit stateChanged();
}

void TailscaleWorkstations::fail(const QString& state)
{
    const int token = m_Token;
    stopProcess(); m_Expiry.stop(); m_Fresh = false; m_State = state;
    const bool signedOut = state == QStringLiteral("needs-login") || state == QStringLiteral("needs-approval");
    if (signedOut) { m_Workstations.clear(); m_Identity.clear(); }
    emit stateChanged();
    if (signedOut) emit identityChanged();
    emit catalogFailed(token, state);
}

void TailscaleWorkstations::finish()
{
    m_Output += m_Process->readAllStandardOutput();
    auto snapshot = parseStatus(m_Output, m_StudioDnsSuffix);
    if (!setupPermitsConnection() || m_RequestAge.elapsed() >= TimeoutMs || !snapshot.valid) {
        fail(m_RequestAge.elapsed() >= TimeoutMs ? QStringLiteral("unavailable") : snapshot.state);
        return;
    }
    const auto permit = studioPermit();
    if (permit && !permit->development) {
        // Tailscale supplies reachability; signed studio setup identifies the
        // supported hosts. Never add a catalog host absent from the peer view.
        auto& entries = snapshot.workstations;
        entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const QVariant& value) {
            return !permit->profile.workstations.contains(value.toMap().value("id").toString());
        }), entries.end());
        snapshot.state = entries.isEmpty() ? QStringLiteral("no-shared-workstations") : QStringLiteral("ready");
    }
    const int token = m_Token;
    const bool changedIdentity = !m_Identity.isEmpty() && snapshot.identity != m_Identity;
    stopProcess();
    m_Identity = snapshot.identity; m_Workstations = snapshot.workstations;
    m_State = snapshot.state; m_Fresh = true;
    m_SnapshotAge.start(); m_SnapshotWallTime = QDateTime::currentMSecsSinceEpoch(); m_Expiry.start();
    emit stateChanged();
    if (changedIdentity) emit identityChanged();
    emit catalogReady(token, m_Workstations, ValidityMs);
}

QVariantMap TailscaleWorkstations::resolve(const QString& nodeId) const
{
    if (!fresh() || busy()) return {};
    for (const auto& value : m_Workstations) {
        auto entry = value.toMap();
        if (entry.value("id").toString() == nodeId && entry.value("status") == QStringLiteral("ready")) {
            entry.insert(QStringLiteral("identity"), m_Identity);
            return entry;
        }
    }
    return {};
}
