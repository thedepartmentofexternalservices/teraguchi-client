#pragma once

#include <QByteArray>
#include <QRect>
#include <QSize>
#include <QVector>
#include <algorithm>

// Process-local identity only: never serialize these records to settings or logs.
namespace MacDisplayBinding {
struct Display {
    quint32 id = 0;
    QByteArray identity;
    quint64 generation = 0;
    QRect bounds;
    QSize pixels;
    QSize nativePixels;
    quint32 mode = 0;
    double refresh = 0;
    double rotation = 0;
    bool mirrored = false;
    bool operator==(const Display& other) const {
        return id == other.id && identity == other.identity && generation == other.generation &&
            bounds == other.bounds && pixels == other.pixels && nativePixels == other.nativePixels &&
            mode == other.mode && refresh == other.refresh && rotation == other.rotation && mirrored == other.mirrored;
    }
};
struct Selection {
    QVector<Display> outputs;
    quint32 primary = 0;
};
inline bool usable(const Display& display)
{
    return display.id && !display.identity.isEmpty() && display.bounds.isValid() &&
        display.pixels.isValid() && display.nativePixels.isValid() && !display.mirrored && display.rotation == 0;
}
inline bool validInventory(const QVector<Display>& inventory)
{
    if (inventory.isEmpty()) return false;
    for (int i = 0; i < inventory.size(); ++i) {
        for (int j = i + 1; j < inventory.size(); ++j) {
            if (inventory[i].id == inventory[j].id || inventory[i].identity == inventory[j].identity)
                return false;
        }
    }
    return true;
}
inline Selection select(const QVector<Display>& inventory, const QRect& launcherScreen, int count)
{
    Selection selected;
    if (!validInventory(inventory) || (count != 1 && count != 2) ||
            (count == 2 && inventory.size() != 2)) return {};
    for (const auto& display : inventory) {
        if (display.bounds == launcherScreen) {
            if (selected.primary || !usable(display)) return {};
            selected.primary = display.id;
        }
    }
    if (!selected.primary) return {};
    for (const auto& display : inventory) {
        if (count == 2 || display.id == selected.primary) {
            if (!usable(display)) return {};
            selected.outputs.append(display);
        }
    }
    std::sort(selected.outputs.begin(), selected.outputs.end(), [](const auto& a, const auto& b) {
        return a.bounds.x() < b.bounds.x();
    });
    if (count == 2) {
        const auto& a = selected.outputs[0].bounds;
        const auto& b = selected.outputs[1].bounds;
        if (a.x() + a.width() > b.x() || a.y() >= b.y() + b.height() || b.y() >= a.y() + a.height()) return {};
    }
    return selected;
}
inline bool current(const Selection& selection, const QVector<Display>& inventory)
{
    if (selection.outputs.isEmpty() || !validInventory(inventory)) return false;
    const auto canonical = select(selection.outputs, [&] {
        for (const auto& display : selection.outputs) if (display.id == selection.primary) return display.bounds;
        return QRect();
    }(), selection.outputs.size());
    if (canonical.outputs != selection.outputs || !canonical.primary) return false;
    for (const auto& expected : selection.outputs) {
        if (std::count(inventory.begin(), inventory.end(), expected) != 1) return false;
    }
    return true;
}
struct Surface { quint32 id; QRect bounds; };
inline QVector<quint32> resolve(const Selection& selection, const QVector<Surface>& surfaces)
{
    QVector<quint32> result;
    if (selection.outputs.isEmpty()) return {};
    for (const auto& display : selection.outputs) {
        quint32 found = 0;
        for (const auto& surface : surfaces) {
            if (surface.bounds != display.bounds) continue;
            if (found || !surface.id) return {};
            found = surface.id;
        }
        if (!found || result.contains(found)) return {};
        result.append(found);
    }
    return result;
}
QVector<Display> read();
inline bool current(const Selection& selection) { return current(selection, read()); }
}
