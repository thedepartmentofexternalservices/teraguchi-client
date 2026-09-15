#pragma once

#include "settings/streamingpreferences.h"

#include <QMap>
#include <QString>

class GlobalCommandLineParser
{
public:
    enum ParseResult {
        NormalStartRequested,
        StreamRequested,
        WorkstationsRequested,
    };

    GlobalCommandLineParser();
    virtual ~GlobalCommandLineParser();

    ParseResult parse(const QStringList &args);
    QString studioConfigPath() const { return m_StudioConfigPath; }
    QString studioDnsSuffix() const { return m_StudioDnsSuffix; }
private:
    QString m_StudioDnsSuffix;
    QString m_StudioConfigPath;

};

class StreamCommandLineParser
{
public:
    StreamCommandLineParser();
    virtual ~StreamCommandLineParser();

    void parse(const QStringList &args, StreamingPreferences *preferences);

    QString getHost() const;
    QString getAppName() const;
    QString getPlankUsername() const;
    QString takePlankPassword();

private:
    QString m_Host;
    QString m_AppName;
    QString m_PlankUsername;
    QString m_PlankPassword;
    QMap<QString, StreamingPreferences::WindowMode> m_WindowModeMap;
    QMap<QString, StreamingPreferences::AudioConfig> m_AudioConfigMap;
    QMap<QString, StreamingPreferences::CaptureSysKeysMode> m_CaptureSysKeysModeMap;
};
