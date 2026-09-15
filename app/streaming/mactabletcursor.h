#pragma once

#include <QImage>
#include <SDL3/SDL.h>
#include <memory>

// Main-thread, input-transparent overlay attached to one presentation window.
// Never moves the native pointer or transforms the outgoing pen coordinates.
class MacTabletCursor
{
public:
    static std::unique_ptr<MacTabletCursor> create(SDL_Window* parentWindow);
    ~MacTabletCursor();
    void dispatchPending();
    bool isAttachedTo(SDL_Window* parentWindow) const;
    void setImage(const QImage& image, int hotspotX, int hotspotY);
    void setPosition(int hotspotX, int hotspotY);
    void setVisible(bool visible);

private:
    class Impl;
    explicit MacTabletCursor(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_Impl;
};
