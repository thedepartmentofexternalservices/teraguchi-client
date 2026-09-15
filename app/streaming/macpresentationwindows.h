#pragma once
#include <SDL3/SDL.h>
#include <QRect>

namespace MacPresentationWindows {
// The two windows stay in the desktop space. No exclusive mode or fullscreen
// Space: either transition must preserve both output surfaces.
inline QRect frame(const QRect& display, bool fullscreen)
{
    if (fullscreen) return display;
    const QSize size(qMax(1, display.width() * 4 / 5), qMax(1, display.height() * 4 / 5));
    return QRect(display.center() - QPoint(size.width() / 2, size.height() / 2), size);
}
bool configure(SDL_Window* window);
inline bool place(SDL_Window* window, SDL_DisplayID id, const QRect& bounds, bool fullscreen)
{
    if (!window || !id || !bounds.isValid() || (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)) return false;
    const auto target = frame(bounds, fullscreen);
    const bool placed = SDL_SetWindowBordered(window, !fullscreen) &&
        SDL_SetWindowResizable(window, !fullscreen) &&
        SDL_SetWindowSize(window, target.width(), target.height()) &&
        SDL_SetWindowPosition(window, target.x(), target.y()) && SDL_SyncWindow(window) &&
        SDL_GetDisplayForWindow(window) == id;
    int x, y, width, height;
    return placed && SDL_GetWindowPosition(window, &x, &y) && SDL_GetWindowSize(window, &width, &height) &&
        QRect(x, y, width, height) == target && configure(window);
}
}
