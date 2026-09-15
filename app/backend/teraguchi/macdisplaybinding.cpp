#include "macdisplaybinding.h"
#ifdef Q_OS_MACOS
#include <CoreGraphics/CoreGraphics.h>
#include <ColorSync/ColorSync.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <QHash>
#include <QSet>
#include <QMutex>
#include <QMutexLocker>

namespace {
// Callbacks can arrive off the UI thread. Keep this observer alive until process
// exit, including after the model/session that first used it has been destroyed.
struct Changes {
    QMutex mutex;
    QHash<quint32, quint64> generations;
    QSet<quint32> pending;
    bool ready = false;
    Changes() {
        ready = CGDisplayRegisterReconfigurationCallback([](CGDirectDisplayID id,
                    CGDisplayChangeSummaryFlags flags, void* context) {
            auto& changes = *static_cast<Changes*>(context);
            QMutexLocker lock(&changes.mutex);
            if (flags & kCGDisplayBeginConfigurationFlag) changes.pending.insert(id);
            else {
                changes.pending.remove(id);
                // Shape notifications also reach unchanged displays. Identity
                // epochs advance only for a change to this particular output.
                if (flags & ~(kCGDisplayBeginConfigurationFlag | kCGDisplayDesktopShapeChangedFlag))
                    ++changes.generations[id];
            }
        }, this) == kCGErrorSuccess;
    }
};
Changes& changes() { static auto* value = new Changes; return *value; }
}
#endif

QVector<MacDisplayBinding::Display> MacDisplayBinding::read()
{
#ifdef Q_OS_MACOS
    auto& observer = changes();
    if (!observer.ready) return {};
    QHash<quint32, quint64> before;
    { QMutexLocker lock(&observer.mutex); if (!observer.pending.isEmpty()) return {}; before = observer.generations; }
    constexpr uint32_t limit = 32;
    CGDirectDisplayID ids[limit];
    uint32_t count = 0;
    if (CGGetOnlineDisplayList(limit, ids, &count) != kCGErrorSuccess || count == limit) return {};
    QVector<Display> result;
    for (uint32_t i = 0; i < count; ++i) {
        Display display;
        display.id = ids[i];
        display.generation = before.value(display.id);
        CFUUIDRef uuid = CGDisplayCreateUUIDFromDisplayID(display.id);
        if (!uuid) return {};
        auto bytes = CFUUIDGetUUIDBytes(uuid);
        display.identity = QByteArray(reinterpret_cast<const char*>(&bytes), sizeof(bytes));
        CFRelease(uuid);
        const auto bounds = CGDisplayBounds(display.id);
        display.bounds = QRect(bounds.origin.x, bounds.origin.y, bounds.size.width, bounds.size.height);
        display.mirrored = CGDisplayIsInMirrorSet(display.id) || !CGDisplayIsActive(display.id);
        display.rotation = CGDisplayRotation(display.id);
        CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display.id);
        if (!mode) return {};
        display.mode = CGDisplayModeGetIODisplayModeID(mode);
        display.pixels = QSize(CGDisplayModeGetPixelWidth(mode), CGDisplayModeGetPixelHeight(mode));
        display.refresh = CGDisplayModeGetRefreshRate(mode);
        CFRelease(mode);
        CFArrayRef modes = CGDisplayCopyAllDisplayModes(display.id, nullptr);
        if (!modes) return {};
        for (CFIndex j = 0; j < CFArrayGetCount(modes); ++j) {
            auto candidate = static_cast<CGDisplayModeRef>(const_cast<void*>(CFArrayGetValueAtIndex(modes, j)));
            if (CGDisplayModeGetIOFlags(candidate) & kDisplayModeNativeFlag) {
                const QSize native(CGDisplayModeGetPixelWidth(candidate), CGDisplayModeGetPixelHeight(candidate));
                if (display.nativePixels.isValid() && display.nativePixels != native) {
                    CFRelease(modes);
                    return {}; // Do not guess between conflicting native resolutions.
                }
                display.nativePixels = native;
            }
        }
        CFRelease(modes);
        result.append(display);
    }
    { QMutexLocker lock(&observer.mutex); if (before != observer.generations) return {}; }
    return result;
#else
    return {};
#endif
}
