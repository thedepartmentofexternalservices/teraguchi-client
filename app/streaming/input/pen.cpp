#include "input.h"
#include "macpen.h"
#include "streaming/session.h"

void SdlInputHandler::initializeMacPen()
{
    if (m_MacPenInput) m_MacPenInput->suspend();
    m_MacPenInput = std::make_unique<MacPenInput>(
        [](const MacPenInput::Packet& packet) {
            return LiSendPenEvent(packet.action, packet.tool, packet.buttons,
                                  packet.x, packet.y, packet.pressure,
                                  0.0f, 0.0f, packet.rotation, packet.tilt) == 0;
        },
        [this](SDL_WindowID id, float x, float y, float& normalizedX, float& normalizedY) {
            if (!isCaptureActive() || m_PenToolbarActive) return false;
            SDL_Window* window = presentationWindow(id);
            if (!window || !(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)) return false;
            const auto* output = presentationOutput(window);
            if (!output) return false;
            int width = 0, height = 0;
            SDL_GetWindowSize(window, &width, &height);
            const QSize size = streamDimensions();
            QPointF point;
            if (width <= 0 || height <= 0 || size.width() <= 0 || size.height() <= 0 ||
                    !PlankPresentation::mapWindowPointToStream(
                        QPointF(x, y), QSize(width, height), size,
                        m_PresentationLayout.canvasSize, output->canvasRect, point, false)) return false;
            normalizedX = float(point.x() / size.width());
            normalizedY = float(point.y() / size.height());
            return true;
        },
        [] { Session::get()->rejectPenInput(); },
        [this](SDL_PenID pen, SDL_WindowID window, float x, float y,
                SDL_PenInputFlags state, Uint64 timestamp) {
            SDL_Window* target = presentationWindow(window);
            if (!target || !(SDL_GetWindowFlags(target) & SDL_WINDOW_INPUT_FOCUS)) return false;
            return Session::get()->routeMacPenToToolbar(pen, window, x, y, state, timestamp);
        }, [] { Session::get()->resetMacPenToolbar(); },
        [this](bool remote) {
            if (!remote) {
                activateCompositorCursor();
                return;
            }
            // This callback runs on the SDL main thread, after the complete
            // sample is accepted. Use the existing host cursor channel; never
            // guess Flame margins or warp the Mac pointer feeding pen events.
            m_TabletCursorActivationPending.store(true);
            applyPendingTabletCursorActivation();
        });
}

void SdlInputHandler::beforePenEvent(const SDL_Event& event)
{
    if (m_MacPenInput) m_MacPenInput->beforeEvent(event);
}

void SdlInputHandler::handlePenEvent(const SDL_Event& event)
{
    if (m_MacPenInput) m_MacPenInput->handle(event);
}

void SdlInputHandler::flushPenInput()
{
    if (m_MacPenInput) m_MacPenInput->flush();
}

bool SdlInputHandler::hasPendingPenInput() const
{
    return m_MacPenInput && m_MacPenInput->pending();
}
