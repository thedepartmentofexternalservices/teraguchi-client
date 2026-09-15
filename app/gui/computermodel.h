#include "backend/computermanager.h"
#include "streaming/session.h"

#include <QAbstractListModel>
#include "backend/teraguchi/tailscaleworkstations.h"

class ComputerModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool relayWakeEnabled READ relayWakeEnabled CONSTANT)

    enum Roles
    {
        NameRole = Qt::UserRole,
        OnlineRole,
        AuthorizedRole,
        StatusUnknownRole,
        PlankHostVersionRole,
        ManualBookmarkRole,
        AddressRole
    };

public:
    explicit ComputerModel(QObject* object = nullptr);

    // Must be called before any QAbstractListModel functions
    Q_INVOKABLE void initialize(ComputerManager* computerManager);

    QVariant data(const QModelIndex &index, int role) const override;

    int rowCount(const QModelIndex &parent) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void deleteComputer(int computerIndex);

    Q_INVOKABLE void authenticateComputer(int computerIndex, QString username, QString password);

    Q_INVOKABLE void renameComputer(int computerIndex, QString name);

    Q_INVOKABLE void requestRelayWake(int computerIndex);

    bool relayWakeEnabled() const;

    Q_INVOKABLE Session* createSessionForPlankDesktop(int computerIndex);

    // Tailscale-selected targets never retain a row across refresh or login.
    Q_INVOKABLE bool prepareAssignedTarget(TailscaleWorkstations* assignments, const QString& nodeId);
    Q_INVOKABLE QVariantMap assignedLoginTarget(TailscaleWorkstations* assignments,
                                               const QString& nodeId) const;
    Q_INVOKABLE QString authenticateAssignedTarget(TailscaleWorkstations* assignments,
                                                const QVariantMap& expected,
                                                QString username, QString password);
    Q_INVOKABLE Session* createAssignedSession(TailscaleWorkstations* assignments,
                                               const QVariantMap& expected, int displays, const QString& requestId);
    Q_INVOKABLE void cancelAssignedAuthentication(const QString& requestId);
    Q_INVOKABLE QString assignedDisplayError(int displays) const;
    Q_INVOKABLE bool assignedInputPermissionsReady() const;

    Q_INVOKABLE int plankScalingChoice(int computerIndex) const;

    Q_INVOKABLE int plankVideoProfile(int computerIndex) const;

    Q_INVOKABLE int plankCaptureSource(int computerIndex) const;

    Q_INVOKABLE QVariantList plankProfileBitratesKbps(
            int computerIndex) const;

    Q_INVOKABLE int plankHostLayoutChoice(int computerIndex) const;

    // -1 is unknown/offline, 0 is physical, and 1 is virtual.
    Q_INVOKABLE int plankHostDisplayPolicy(int computerIndex) const;

    Q_INVOKABLE int plankVirtualMode1Choice(int computerIndex) const;

    Q_INVOKABLE int plankVirtualMode2Choice(int computerIndex) const;

    Q_INVOKABLE bool editComputerBookmark(int computerIndex, QString address,
                                          QString nickname, int scalingChoice,
                                          int hostLayoutChoice,
                                          int virtualMode1Choice,
                                          int virtualMode2Choice,
                                          int videoProfile, int captureSource,
                                          const QVariantList& profileBitratesKbps);

signals:
    void authenticationCompleted(QVariant error);
    void assignedAuthenticationCompleted(QString requestId, QString computerId, QVariant error);

    void relayWakeCompleted(QVariant error);

private slots:
    void handleComputerStateChanged(NvComputer* computer);

    void handleAuthenticationCompleted(NvComputer* computer, QString error);

private:
    QVector<NvComputer*> m_Computers;
    ComputerManager* m_ComputerManager = nullptr;
};
