#pragma once
#include <QStringList>
#ifdef Q_OS_MACOS
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace TeraguchiStartup {
inline QStringList arguments(QStringList input, bool workstationDefault)
{
    if (workstationDefault && input.size() == 1) input.append(QStringLiteral("--workstations"));
    return input;
}
inline QStringList arguments(const QStringList& input)
{
    bool workstationDefault = false;
#if defined(Q_OS_MACOS) && defined(TERAGUCHI_STRICT_VIDEO)
    if (auto bundle = CFBundleGetMainBundle()) {
        const auto value = CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("TeraguchiWorkstationPicker"));
        workstationDefault = value && CFGetTypeID(value) == CFBooleanGetTypeID() &&
            CFBooleanGetValue(static_cast<CFBooleanRef>(value));
    }
#endif
    return arguments(input, workstationDefault);
}
}
