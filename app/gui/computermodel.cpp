#include <QScreen>
#include "computermodel.h"
#include "backend/relaywakeclient.h"
#include "backend/teraguchi/assignmenttarget.h"
#include "backend/teraguchi/macinputaccess.h"
#include "settings/plankclientpolicy.h"

#include <utility>
#include <QUuid>

namespace {
QString hostLayoutFromChoice(int choice)
{
    switch (choice) {
    case 0: return QString::fromLatin1(NvOutputTopology::MatchClientHostLayout);
    case 1: return QString::fromLatin1(NvOutputTopology::PhysicalHostLayout);
    case 2: return QString::fromLatin1(NvOutputTopology::SingleHostLayout);
    case 3: return QString::fromLatin1(NvOutputTopology::DualHorizontalHostLayout);
    default: return QString();
    }
}

QString virtualModeFromChoice(int choice)
{
    return NvOutputTopology::qualifiedVirtualModes().value(choice);
}
}

ComputerModel::ComputerModel(QObject* object)
    : QAbstractListModel(object) {}

void ComputerModel::initialize(ComputerManager* computerManager)
{
    m_ComputerManager = computerManager;
    connect(m_ComputerManager, &ComputerManager::computerStateChanged,
            this, &ComputerModel::handleComputerStateChanged);
    connect(m_ComputerManager, &ComputerManager::authenticationCompleted,
            this, &ComputerModel::handleAuthenticationCompleted);

    connect(m_ComputerManager, &ComputerManager::assignedAuthenticationCompleted,
            this, &ComputerModel::assignedAuthenticationCompleted);
    m_Computers = m_ComputerManager->getComputers();
}

QVariant ComputerModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    Q_ASSERT(index.row() < m_Computers.count());

    NvComputer* computer = m_Computers[index.row()];
    QReadLocker lock(&computer->lock);

    switch (role) {
    case NameRole:
        return computer->name;
    case OnlineRole:
        return computer->state == NvComputer::CS_ONLINE;
    case AuthorizedRole:
        return computer->authorizationState == NvComputer::AS_AUTHORIZED;
    case StatusUnknownRole:
        return computer->state == NvComputer::CS_UNKNOWN;
    case PlankHostVersionRole:
        return computer->plankHostMetadataVersion >= 1 ?
                    computer->plankHostVersion : QString();
    case ManualBookmarkRole:
        return computer->manualBookmark;
    case AddressRole:
        // New PLANK bookmarks have a durable manual address, but
        // workstation records created before bookmarks do not. Never expose
        // NvAddress's diagnostic <NULL> sentinel in the main workstation row.
        if (!computer->manualAddress.isNull()) {
            return computer->manualAddress.toString();
        }
        if (!computer->activeAddress.isNull()) {
            return computer->activeAddress.toString();
        }
        if (!computer->localAddress.isNull()) {
            return computer->localAddress.toString();
        }
        if (!computer->remoteAddress.isNull()) {
            return computer->remoteAddress.toString();
        }
        if (!computer->ipv6Address.isNull()) {
            return computer->ipv6Address.toString();
        }
        return QString();
    default:
        return QVariant();
    }
}

int ComputerModel::rowCount(const QModelIndex& parent) const
{
    // We should not return a count for valid index values,
    // only the parent (which will not have a "valid" index).
    if (parent.isValid()) {
        return 0;
    }

    return m_Computers.count();
}

QHash<int, QByteArray> ComputerModel::roleNames() const
{
    QHash<int, QByteArray> names;

    names[NameRole] = "name";
    names[OnlineRole] = "online";
    names[AuthorizedRole] = "authorized";
    names[StatusUnknownRole] = "statusUnknown";
    names[PlankHostVersionRole] = "plankHostVersion";
    names[ManualBookmarkRole] = "manualBookmark";
    names[AddressRole] = "address";

    return names;
}

Session* ComputerModel::createSessionForPlankDesktop(int computerIndex)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);
    for (NvApp& app : computer->appList) {
        if (app.name == QStringLiteral("Desktop")) {
            return new Session(computer, app, nullptr, m_ComputerManager);
        }
    }

    return nullptr;
}

int ComputerModel::plankScalingChoice(int computerIndex) const
{
    Q_ASSERT(computerIndex >= 0 && computerIndex < m_Computers.count());
    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);
    return computer->plankScalingMode == NvOutputTopology::NativeScalingMode ? 0 : 1;
}

int ComputerModel::plankVideoProfile(int computerIndex) const
{
    Q_ASSERT(computerIndex >= 0 && computerIndex < m_Computers.count());
    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);
    return computer->plankVideoProfile;
}

int ComputerModel::plankCaptureSource(int computerIndex) const
{
    Q_ASSERT(computerIndex >= 0 && computerIndex < m_Computers.count());
    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);
    return computer->plankCaptureSource;
}

QVariantList ComputerModel::plankProfileBitratesKbps(
        int computerIndex) const
{
    Q_ASSERT(computerIndex >= 0 && computerIndex < m_Computers.count());
    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);
    return StreamingPreferences::plankProfileBitratesToVariantList(
                computer->plankProfileBitratesKbps);
}

int ComputerModel::plankHostLayoutChoice(int computerIndex) const
{
    Q_ASSERT(computerIndex >= 0 && computerIndex < m_Computers.count());
    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);
    if (computer->plankHostLayout == NvOutputTopology::PhysicalHostLayout) {
        return 1;
    }
    if (computer->plankHostLayout == QStringLiteral("fixed")) {
        return 1; // Mac model: Match Client, then fixed virtual display.
    }
    if (computer->plankHostLayout == NvOutputTopology::SingleHostLayout) {
        return 2;
    }
    if (computer->plankHostLayout == NvOutputTopology::DualHorizontalHostLayout) {
        return 3;
    }
    return 0;
}

int ComputerModel::plankHostDisplayPolicy(int computerIndex) const
{
    Q_ASSERT(computerIndex >= 0 && computerIndex < m_Computers.count());
    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);
    if (computer->state != NvComputer::CS_ONLINE ||
            computer->authorizationState != NvComputer::AS_AUTHORIZED ||
            !computer->outputTopology.displayPolicyKnown()) {
        return -1;
    }
    return computer->outputTopology.allowedLayoutKinds.contains(
                NvOutputTopology::PhysicalHostLayout) ? 1 : 0;
}

int ComputerModel::plankVirtualMode1Choice(int computerIndex) const
{
    Q_ASSERT(computerIndex >= 0 && computerIndex < m_Computers.count());
    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);
    return NvOutputTopology::qualifiedVirtualModes().indexOf(
                computer->plankVirtualMode1);
}

int ComputerModel::plankVirtualMode2Choice(int computerIndex) const
{
    Q_ASSERT(computerIndex >= 0 && computerIndex < m_Computers.count());
    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);
    return NvOutputTopology::qualifiedVirtualModes().indexOf(
                computer->plankVirtualMode2);
}

bool ComputerModel::editComputerBookmark(int computerIndex, QString address,
                                         QString nickname, int scalingChoice,
                                         int hostLayoutChoice,
                                         int virtualMode1Choice,
                                         int virtualMode2Choice,
                                         int videoProfile, int captureSource,
                                         const QVariantList& profileBitratesKbps)
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return false;
    }

    NvComputer* computer = m_Computers[computerIndex];
    const QString hostLayout = hostLayoutFromChoice(hostLayoutChoice);
    const QString virtualMode1 = virtualModeFromChoice(virtualMode1Choice);
    const QString virtualMode2 = virtualModeFromChoice(virtualMode2Choice);
    if (hostLayout.isEmpty() || virtualMode1.isEmpty() || virtualMode2.isEmpty()) {
        return false;
    }
    if (scalingChoice < 0 || scalingChoice > 1) {
        return false;
    }
    const QString scalingMode = scalingChoice == 0 ?
                QString::fromLatin1(NvOutputTopology::NativeScalingMode) :
                QString::fromLatin1(NvOutputTopology::ScaledSpanMode);

    return m_ComputerManager->editManualBookmark(computer, std::move(address),
                                                  std::move(nickname), scalingMode,
                                                  hostLayout,
                                                  virtualMode1, virtualMode2,
                                                  videoProfile, captureSource,
                                                  profileBitratesKbps);
}

void ComputerModel::deleteComputer(int computerIndex)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    beginRemoveRows(QModelIndex(), computerIndex, computerIndex);

    // m_Computer[computerIndex] will be deleted by this call
    m_ComputerManager->deleteHost(m_Computers[computerIndex]);

    // Remove the now invalid item
    m_Computers.removeAt(computerIndex);

    endRemoveRows();
}

void ComputerModel::renameComputer(int computerIndex, QString name)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    m_ComputerManager->renameHost(m_Computers[computerIndex], name);
}

bool ComputerModel::relayWakeEnabled() const
{
    return PlankClientPolicy().relayWakeEnabled();
}

void ComputerModel::requestRelayWake(int computerIndex)
{
    // Recheck policy at dispatch, even if an already-open menu is still visible.
    if (!relayWakeEnabled()) {
        emit relayWakeCompleted(tr("Wake PC is disabled by administrator policy."));
        return;
    }

    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        emit relayWakeCompleted(tr("The selected workstation bookmark is unavailable."));
        return;
    }

    NvComputer* computer = m_Computers[computerIndex];
    QString address;
    {
        QReadLocker lock(&computer->lock);
        if (!computer->manualBookmark || computer->manualAddress.isNull()) {
            emit relayWakeCompleted(tr("Wake PC requires a manually configured bookmark."));
            return;
        }
        if (computer->state != NvComputer::CS_OFFLINE) {
            emit relayWakeCompleted(tr("Wake PC requires an offline workstation."));
            return;
        }
        address = computer->manualAddress.address();
    }

    auto* request = new RelayWakeClient(
                address, PlankClientPolicy().relayWakePort(), this);
    connect(request, &RelayWakeClient::completed,
            this, &ComputerModel::relayWakeCompleted);
    request->start();
}

void ComputerModel::authenticateComputer(int computerIndex, QString username,
                                         QString password)
{
    Q_ASSERT(computerIndex < m_Computers.count());
    m_ComputerManager->authenticateHost(m_Computers[computerIndex],
                                        std::move(username), std::move(password));
}

void ComputerModel::handleAuthenticationCompleted(NvComputer*, QString error)
{
    emit authenticationCompleted(error.isEmpty() ? QVariant() : error);
}

void ComputerModel::handleComputerStateChanged(NvComputer* computer)
{
    QVector<NvComputer*> newComputerList = m_ComputerManager->getComputers();

    // Reset the model if the structural layout of the list has changed
    if (m_Computers != newComputerList) {
        beginResetModel();
        m_Computers = newComputerList;
        endResetModel();
    }
    else {
        // Let the view know that this specific computer changed
        int index = m_Computers.indexOf(computer);
        emit dataChanged(createIndex(index, 0), createIndex(index, 0));
    }
}

bool ComputerModel::prepareAssignedTarget(TailscaleWorkstations* assignments, const QString& nodeId)
{
    if (!assignments || !m_ComputerManager) return false;
    const auto peer = assignments->resolve(nodeId);
    if (peer.isEmpty()) return false;
    const QString address = peer.value(QStringLiteral("address")).toString();
    for (auto* computer : m_ComputerManager->getComputers()) {
        QReadLocker lock(&computer->lock);
        if (computer->manualBookmark && QHostAddress(computer->manualAddress.address()) == QHostAddress(address))
            return true; // Existing choices and pins are never overwritten by discovery.
    }
    // Runs only after the artist selects Connect. The normal poller verifies
    // PLANK metadata before assignedLoginTarget will expose a credential target.
    m_ComputerManager->addNewHostManually(address, peer.value(QStringLiteral("name")).toString(),
            0, 9, 1, 1, StreamingPreferences::PLANK_PROFILE_NVENC_HEVC_10BIT_444,
            StreamingPreferences::PLANK_CAPTURE_X11_NATIVE10);
    return true;
}

QVariantMap ComputerModel::assignedLoginTarget(TailscaleWorkstations* assignments,
                                              const QString& nodeId) const
{
    if (!assignments || !m_ComputerManager) return {};
    auto peer = assignments->resolve(nodeId);
    if (peer.isEmpty()) return {};
    const auto setup = assignments->studioPermit();
    if (!setup || setup->development || !setup->valid() || !setup->profile.workstations.contains(nodeId))
        return {{QStringLiteral("trustError"), tr("Import current studio setup containing this workstation’s trusted certificate before signing in.")}};
    const auto trustedHost = setup->profile.workstations.value(nodeId).hostId;
    const QHostAddress address(peer.value(QStringLiteral("address")).toString());
    QVariantMap found;
    // Resolve against the current manager, independent of cached view ordering.
    for (auto* computer : m_ComputerManager->getComputers()) {
        QReadLocker lock(&computer->lock);
        if (!computer->manualBookmark || computer->state != NvComputer::CS_ONLINE ||
                !computer->plankAuthentication || computer->plankHostMetadataVersion < 1 ||
                NvOutputTopology::hostPlatform(computer->plankTopologyVersion, computer->plankFeatureFlags) != 1 ||
                computer->serverUuid.isEmpty() ||
                QHostAddress(computer->manualAddress.address()) != address ||
                computer->activeAddress != computer->manualAddress) continue;
        if (computer->serverUuid != trustedHost)
            return {{QStringLiteral("trustError"), tr("The workstation identity differs from studio setup. Ask the studio to verify it before signing in.")}};
        if (!found.isEmpty()) return {}; // Ambiguous endpoints must be resolved by setup.
        found = peer;
        found.insert(QStringLiteral("computerId"), computer->uuid);
        found.insert(QStringLiteral("hostId"), computer->serverUuid);
    }
    return found;
}

QString ComputerModel::authenticateAssignedTarget(TailscaleWorkstations* assignments,
                                               const QVariantMap& expected,
                                               QString username, QString password)
{
    const auto displayToken = expected.value(QStringLiteral("displayToken")).toString();
    if (!assignments || !assignments->studioPermit() || !assignments->studioPermit()->valid() ||
            !assignedInputPermissionsReady() || !assignedDisplaysCurrent(displayToken)) {
        password.fill(QChar(0)); return {};
    }
    const auto current = assignedLoginTarget(assignments, expected.value(QStringLiteral("id")).toString());
    if (!TeraguchiAssignment::matches(expected, current)) {
        password.fill(QChar('\0'));
        return {};
    }
    for (auto* computer : m_ComputerManager->getComputers()) {
        NvAddress address;
        QString id;
        { QReadLocker lock(&computer->lock); id = computer->uuid; address = computer->activeAddress; }
        if (id == current.value(QStringLiteral("computerId")).toString()) {
            if (QHostAddress(address.address()) != QHostAddress(current.value(QStringLiteral("address")).toString())) return {};
            auto trust = std::make_shared<TeraguchiStudio::HostTrust>();
            trust->setup = assignments->studioPermit();
            trust->nodeId = current.value("id").toString();
            trust->hostId = current.value("hostId").toString();
            trust->address = address.address(); trust->port = address.port();
            if (!trust->valid()) { password.fill(QChar(0)); return {}; }
            const auto requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            m_AuthenticationDisplays.insert(requestId, displayToken);
            m_AuthenticationSetup.insert(requestId, assignments->studioPermit());
            m_ComputerManager->authenticateHost(computer, std::move(username), std::move(password),
                                                 address, current.value(QStringLiteral("hostId")).toString(), requestId, trust);
            return requestId;
        }
    }
    password.fill(QChar('\0'));
    return {};
}

bool ComputerModel::assignedInputPermissionsReady() const
{
    return MacInputAccess::query().ready();
}

QString ComputerModel::assignedDisplayError(int displays) const
{
    return displays == 1 || displays == 2 ? QString() : tr("Select one or two displays.");
}

QVariantMap ComputerModel::prepareAssignedDisplays(int displays, QWindow* window)
{
    m_DisplayToken.clear();
    m_AssignedDisplays = {};
    const auto error = assignedDisplayError(displays);
    if (!error.isEmpty()) return {{QStringLiteral("error"), error}};
    if (window && window->screen())
        m_AssignedDisplays = MacDisplayBinding::select(MacDisplayBinding::read(), window->screen()->geometry(), displays);
    if (m_AssignedDisplays.outputs.size() != displays)
        return {{QStringLiteral("error"), tr("The selected displays cannot be bound. Use independent, unrotated displays arranged side by side, then try again.")}};
    QVector<NvClientDisplay> outputs;
    for (const auto& display : m_AssignedDisplays.outputs) outputs.append({display.bounds, display.nativePixels});
    QString layout, reason;
    QStringList modes;
    if (!NvOutputTopology::resolveClientDisplayLayout(outputs, layout, modes, &reason)) {
        m_AssignedDisplays = {};
        return {{QStringLiteral("error"), reason}};
    }
    m_DisplayToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return {{QStringLiteral("token"), m_DisplayToken}};
}

bool ComputerModel::assignedDisplaysCurrent(const QString& token) const
{
    return !token.isEmpty() && token == m_DisplayToken && MacDisplayBinding::current(m_AssignedDisplays);
}

void ComputerModel::cancelAssignedDisplays(const QString& token)
{
    if (token != m_DisplayToken) return;
    m_DisplayToken.clear();
    m_AssignedDisplays = {};
}

Session* ComputerModel::createAssignedSession(TailscaleWorkstations* assignments,
                                             const QVariantMap& expected, int displays, const QString& requestId)
{
#ifndef TERAGUCHI_STRICT_VIDEO
    Q_UNUSED(assignments); Q_UNUSED(expected); Q_UNUSED(displays); Q_UNUSED(requestId);
    return nullptr;
#else
    const auto displayToken = expected.value(QStringLiteral("displayToken")).toString();
    if (!assignments || !assignments->studioPermit() || !assignments->studioPermit()->valid() ||
            m_AuthenticationSetup.value(requestId) != assignments->studioPermit() ||
            !assignedInputPermissionsReady() || !assignedDisplayError(displays).isEmpty() ||
            !assignedDisplaysCurrent(displayToken) || m_AssignedDisplays.outputs.size() != displays ||
            m_AuthenticationDisplays.value(requestId) != displayToken) return nullptr;
    m_AuthenticationDisplays.remove(requestId);
    m_AuthenticationSetup.remove(requestId);
    const auto current = assignedLoginTarget(assignments, expected.value(QStringLiteral("id")).toString());
    if (!TeraguchiAssignment::matches(expected, current)) return nullptr;
    QString username, password;
    auto computer = m_ComputerManager->takeAssignedAuthentication(requestId, username, password);
    if (!computer) return nullptr;
    if (!computer->assignedHostTrust || !computer->assignedHostTrust->valid() ||
            computer->assignedHostTrust->setup != assignments->studioPermit() ||
            computer->assignedHostTrust->nodeId != current.value("id").toString()) {
        password.fill(QChar(0)); return nullptr;
    }
    if (computer->uuid != current.value("computerId").toString() ||
            computer->serverUuid != current.value("hostId").toString() ||
            QHostAddress(computer->activeAddress.address()) != QHostAddress(current.value("address").toString())) {
        password.fill(QChar(0));
        return nullptr;
    }
    for (auto& app : computer->appList) {
        if (app.name == QStringLiteral("Desktop")) {
            auto* session = new Session(computer.get(), app);
            session->bindAssignedTarget(assignments, current, displays, assignments->studioPermit());
            session->bindAssignedDisplays(m_AssignedDisplays);
            session->setAssignedCredentials(std::move(username), std::move(password));
            return session;
        }
    }
    password.fill(QChar(0));
    return nullptr;
#endif
}

void ComputerModel::cancelAssignedAuthentication(const QString& requestId)
{
    m_AuthenticationDisplays.remove(requestId);
    m_AuthenticationSetup.remove(requestId);
    if (m_ComputerManager) m_ComputerManager->cancelAssignedAuthentication(requestId);
}
