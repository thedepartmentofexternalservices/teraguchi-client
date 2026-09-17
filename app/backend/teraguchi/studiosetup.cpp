#include "studiosetup.h"
#ifdef TERAGUCHI_STUDIO_KEY_BUILD
#include "teraguchi-studio-key.h"
#endif
#include "tailscaleworkstations.h"
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <openssl/evp.h>

namespace {
constexpr qint64 MaxBytes = 8192;
const QByteArray Domain("Teraguchi studio setup v1\n");
QString bundledSetupPath() {
#ifdef Q_OS_MACOS
    return QCoreApplication::applicationDirPath() + "/../Resources/studio-setup.teraguchi-studio";
#else
    return {};
#endif
}
QByteArray readFile(const QString& path) {
    QFile file(path);
    if (!QFileInfo(path).isFile() || QFileInfo(path).isSymLink() || !file.open(QIODevice::ReadOnly) || file.size() > MaxBytes) return {};
    return file.read(MaxBytes + 1);
}
bool fields(const QJsonObject& object, const QStringList& expected) {
    auto wanted = expected; wanted.sort(); return object.keys() == wanted;
}
QByteArray base64(const QJsonValue& value) {
    if (!value.isString()) return {};
    const auto encoded = value.toString().toLatin1();
    const auto decoded = QByteArray::fromBase64(encoded, QByteArray::AbortOnBase64DecodingErrors);
    return decoded.toBase64() == encoded ? decoded : QByteArray();
}
}

QByteArray TeraguchiStudio::pinnedPublicKey()
{
#ifdef TERAGUCHI_STUDIO_KEY_HEX
    return QByteArray::fromHex(QByteArray(TERAGUCHI_STUDIO_KEY_HEX));
#else
    return {};
#endif
}

TeraguchiStudio::Profile TeraguchiStudio::verify(const QByteArray& envelope, const QByteArray& key, qint64 now, QString* error, bool allowInactive)
{
    const auto reject = [error](const QString& message) { if (error) *error = message; return Profile{}; };
    if (error) error->clear();
    if (key.size() != 32) return reject(QObject::tr("This client has no studio verification key. Ask your administrator for the configured client."));
    if (envelope.isEmpty() || envelope.size() > MaxBytes) return reject(QObject::tr("The setup file is empty or too large."));
    const auto document = QJsonDocument::fromJson(envelope);
    if (!document.isObject() || document.toJson(QJsonDocument::Compact) != envelope.trimmed() ||
            !fields(document.object(), {"payload", "signature"}))
        return reject(QObject::tr("This is not a supported studio setup file."));
    const auto payload = base64(document.object().value("payload"));
    const auto signature = base64(document.object().value("signature"));
    const auto message = Domain + payload;
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> publicKey(
        EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr,
            reinterpret_cast<const unsigned char*>(key.constData()), key.size()), EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (payload.isEmpty() || signature.size() != 64 || !publicKey || !context ||
            EVP_DigestVerifyInit(context.get(), nullptr, nullptr, nullptr, publicKey.get()) != 1 ||
            EVP_DigestVerify(context.get(), reinterpret_cast<const unsigned char*>(signature.constData()), signature.size(),
                             reinterpret_cast<const unsigned char*>(message.constData()), message.size()) != 1)
        return reject(QObject::tr("The studio signature could not be verified. Ask your administrator for a new setup file."));
    const auto content = QJsonDocument::fromJson(payload);
    const auto object = content.object();
    const bool hostTrustVersion = object.value("version") == QJsonValue(2);
    QStringList expectedFields {"version", "revision", "label", "dns_suffix", "issued_at", "expires_at"};
    if (hostTrustVersion) expectedFields.append("workstations");
    if (!content.isObject() || content.toJson(QJsonDocument::Compact) != payload ||
            !fields(object, expectedFields))
        return reject(QObject::tr("The signed setup uses an unsupported format."));
    Profile profile;
    profile.label = object.value("label").toString();
    profile.suffix = object.value("dns_suffix").toString();
    const auto revision = object.value("revision").toDouble();
    profile.revision = object.value("revision").toInteger();
    const auto issued = object.value("issued_at").toString();
    const auto expires = object.value("expires_at").toString();
    const auto issuedDate = QDateTime::fromString(issued, Qt::ISODate);
    const auto expiresDate = QDateTime::fromString(expires, Qt::ISODate);
    if ((!hostTrustVersion && object.value("version") != QJsonValue(1)) || profile.revision < 1 || profile.revision > 2147483647 ||
            revision != profile.revision || profile.label.isEmpty() || profile.label.size() > 80 ||
            profile.label != profile.label.trimmed() || profile.label.contains(QRegularExpression("[\\x00-\\x1f\\x7f<>]")) ||
            profile.suffix.isEmpty() || TailscaleWorkstations::normalizedSuffix(profile.suffix) != profile.suffix ||
            !issuedDate.isValid() || !expiresDate.isValid() ||
            issuedDate.toUTC().toString("yyyy-MM-ddTHH:mm:ss'Z'") != issued ||
            expiresDate.toUTC().toString("yyyy-MM-ddTHH:mm:ss'Z'") != expires)
        return reject(QObject::tr("The signed studio details are invalid."));
    profile.issued = issuedDate.toSecsSinceEpoch(); profile.expires = expiresDate.toSecsSinceEpoch();
    if ((!allowInactive && (profile.issued > now || profile.expires <= now)) || profile.issued <= 0 ||
            profile.expires <= profile.issued || profile.expires - profile.issued > 90 * 86400)
        return reject(QObject::tr("This studio setup is not currently valid. Check your clock or ask your administrator for a new file."));
    if (hostTrustVersion) {
        const auto entries = object.value("workstations");
        if (!entries.isArray() || entries.toArray().isEmpty() || entries.toArray().size() > 8)
            return reject(QObject::tr("The setup must contain one to eight trusted workstations."));
        QSet<QString> hostIds;
        QSet<QByteArray> fingerprints;
        const QRegularExpression identifier(QRegularExpression::anchoredPattern("[A-Za-z0-9_-]{1,64}"));
        const QRegularExpression fingerprint(QRegularExpression::anchoredPattern("[0-9a-f]{64}"));
        for (const auto& value : entries.toArray()) {
            const auto entry = value.toObject();
            const auto node = entry.value("node_id").toString();
            Workstation workstation;
            workstation.hostId = entry.value("host_id").toString();
            const auto pins = entry.value("certificate_sha256");
            if (!value.isObject() || !fields(entry, {"node_id", "host_id", "certificate_sha256"}) ||
                    !identifier.match(node).hasMatch() || !identifier.match(workstation.hostId).hasMatch() ||
                    profile.workstations.contains(node) || hostIds.contains(workstation.hostId) ||
                    !pins.isArray() || pins.toArray().isEmpty() || pins.toArray().size() > 2)
                return reject(QObject::tr("The trusted workstation details are invalid or duplicated."));
            hostIds.insert(workstation.hostId);
            for (const auto& pin : pins.toArray()) {
                const auto hex = pin.toString();
                const auto bytes = QByteArray::fromHex(hex.toLatin1());
                if (!pin.isString() || !fingerprint.match(hex).hasMatch() || fingerprints.contains(bytes))
                    return reject(QObject::tr("Workstation certificate fingerprints are invalid or reused."));
                fingerprints.insert(bytes); workstation.certificates.append(bytes);
            }
            profile.workstations.insert(node, workstation);
        }
    }
    profile.digest = QCryptographicHash::hash(payload, QCryptographicHash::Sha256);
    return profile;
}

StudioSetup::StudioSetup(QObject* parent)
    : StudioSetup(TeraguchiStudio::pinnedPublicKey(),
        (QFile::exists(QDir::currentPath() + "/portable.dat") ? QDir::currentPath() + "/studio-config" :
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)) + "/studio-setup.json",
        [] { return QDateTime::currentSecsSinceEpoch(); }, parent, bundledSetupPath()) {}

StudioSetup::StudioSetup(QByteArray key, QString storage, std::function<qint64()> now, QObject* parent, QString bundledSetup)
    : QObject(parent), m_Key(std::move(key)), m_Storage(std::move(storage)), m_Now(std::move(now))
{
    m_State = canImport() ? QStringLiteral("needed") : QStringLiteral("unconfigured");
    load();
    loadBundled(bundledSetup);
    m_Timer.setInterval(1000);
    connect(&m_Timer, &QTimer::timeout, this, &StudioSetup::refresh);
    m_Timer.start();
}

bool StudioSetup::ready() const
{
    return m_Permit && m_Permit->valid(m_Now(), std::chrono::steady_clock::now());
}

void StudioSetup::error(const QString& message)
{
    m_Message = message;
    emit statusChanged();
}

void StudioSetup::accept(const TeraguchiStudio::Profile& profile)
{
    auto permit = std::make_shared<TeraguchiStudio::Permit>();
    permit->profile = profile; permit->admittedAt = m_Now();
    m_Highest = profile; m_Permit = permit;
    m_State = QStringLiteral("ready"); m_Message.clear();
    emit configurationChanged(); emit statusChanged();
}

void StudioSetup::load()
{
    if (!canImport() || !QFileInfo::exists(m_Storage)) return;
    QString reason;
    const auto profile = TeraguchiStudio::verify(readFile(m_Storage), m_Key, m_Now(), &reason, true);
    if (profile.revision) {
        m_Highest = profile;
        if (profile.issued <= m_Now() && profile.expires > m_Now()) accept(profile);
        else { m_State = QStringLiteral("expired"); m_Message = tr("The saved studio setup is not currently valid. Check your clock or import a current file."); }
    }
    else { m_State = QStringLiteral("invalid"); m_Message = reason; }
}

void StudioSetup::loadBundled(const QString& path)
{
    if (!canImport() || path.isEmpty() || !QFileInfo::exists(path)) return;
    // A damaged saved record cannot establish the revision floor. Leave it for
    // explicit repair instead of silently reverting to an older bundled copy.
    if (m_State == QStringLiteral("invalid")) return;
    QString reason;
    const auto profile = TeraguchiStudio::verify(readFile(path), m_Key, m_Now(), &reason, true);
    if (profile.revision && (profile.revision < m_Highest.revision ||
            (profile.revision == m_Highest.revision && profile.digest == m_Highest.digest))) return;
    if (!profile.revision || profile.workstations.isEmpty()) {
        if (!ready()) m_State = QStringLiteral("invalid");
        error(reason.isEmpty() ? tr("The included studio setup has no trusted workstations. Ask your studio for an updated app.") : reason);
        return;
    }
    // The same signed import path enforces validity, conflicts, private storage
    // and rollback protection. A package cannot replace a newer manual import.
    if (importFile(QUrl::fromLocalFile(path))) qInfo("Bundled studio setup accepted");
    else if (!ready()) m_State = QStringLiteral("invalid");
}

void StudioSetup::setDevelopmentSuffix(const QString& suffix)
{
    if (canImport() || TailscaleWorkstations::normalizedSuffix(suffix).isEmpty()) return;
    auto permit = std::make_shared<TeraguchiStudio::Permit>();
    permit->development = true; permit->profile.suffix = suffix;
    permit->profile.label = tr("Development setup");
    m_Permit = permit; m_State = QStringLiteral("development");
    emit configurationChanged(); emit statusChanged();
}

bool StudioSetup::importFile(const QUrl& url)
{
    if (!canImport()) { error(tr("Ask your administrator for a client with the studio verification key.")); return false; }
    if (!url.isLocalFile() || !url.host().isEmpty()) { error(tr("Choose a local studio setup file.")); return false; }
    const auto envelope = readFile(url.toLocalFile());
    QString reason;
    const auto profile = TeraguchiStudio::verify(envelope, m_Key, m_Now(), &reason);
    if (!profile.revision) { error(reason); return false; }
    if (profile.revision < m_Highest.revision || (profile.revision == m_Highest.revision && profile.digest != m_Highest.digest)) {
        error(tr("This setup is older than the saved version or conflicts with it. Ask your administrator for the latest file.")); return false;
    }
    const auto directory = QFileInfo(m_Storage).absolutePath();
    if (!QDir().mkpath(directory) || QFileInfo(directory).isSymLink() || QFileInfo(m_Storage).isSymLink()) {
        error(tr("The studio setup could not be saved. Check the client’s application data folder.")); return false;
    }
    if (!QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) {
        error(tr("The client’s application data folder could not be secured.")); return false;
    }
    QSaveFile saved(m_Storage);
    if (!saved.open(QIODevice::WriteOnly) || !saved.setPermissions(QFile::ReadOwner | QFile::WriteOwner) ||
            saved.write(envelope) != envelope.size() || !saved.commit()) {
        error(tr("The studio setup could not be saved. Check the client’s application data folder.")); return false;
    }
    accept(profile);
    return true;
}

void StudioSetup::refresh()
{
    if (m_Permit && !ready()) {
        m_Permit.reset(); m_State = QStringLiteral("expired");
        m_Message = tr("Studio setup has expired or the clock changed. Check your clock or import a new file from your administrator.");
        emit configurationChanged(); emit statusChanged();
    }
}
