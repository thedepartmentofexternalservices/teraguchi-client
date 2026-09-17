#include "macpen.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr SDL_PenInputFlags InteractionFlags = SDL_PEN_INPUT_DOWN |
        SDL_PEN_INPUT_BUTTON_1 | SDL_PEN_INPUT_BUTTON_2 | SDL_PEN_INPUT_BUTTON_3;
}

MacPenInput::MacPenInput(Sender sender, Mapper mapper, std::function<void()> failure,
                        LocalHandler local, std::function<void()> resetLocal,
                        std::function<void(bool)> remoteCursor)
    : m_Send(std::move(sender)), m_Map(std::move(mapper)), m_Failure(std::move(failure)),
      m_Local(std::move(local)), m_ResetLocal(std::move(resetLocal)),
      m_RemoteCursor(std::move(remoteCursor))
{}

bool MacPenInput::isPenEvent(Uint32 type)
{
    return type >= SDL_EVENT_PEN_PROXIMITY_IN && type <= SDL_EVENT_PEN_AXIS;
}

bool MacPenInput::isSyntheticMouse(const SDL_Event& event)
{
    if (event.type == SDL_EVENT_MOUSE_MOTION) return event.motion.which == SDL_PEN_MOUSEID;
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP)
        return event.button.which == SDL_PEN_MOUSEID;
    return event.type == SDL_EVENT_MOUSE_WHEEL && event.wheel.which == SDL_PEN_MOUSEID;
}

void MacPenInput::beforeEvent(const SDL_Event& event)
{
    // Synthetic pen mouse events are interleaved inside the Cocoa sample.
    // Local controls receive the assembled sample, never these early events.
    const bool syntheticTouch =
            (event.type == SDL_EVENT_FINGER_DOWN || event.type == SDL_EVENT_FINGER_UP ||
             event.type == SDL_EVENT_FINGER_MOTION || event.type == SDL_EVENT_FINGER_CANCELED) &&
            event.tfinger.touchID == SDL_PEN_TOUCHID;
    if (!isPenEvent(event.type) &&
            !((isSyntheticMouse(event) || syntheticTouch) && event.common.timestamp == m_Timestamp)) {
        flush();
    }
}

bool MacPenInput::send(Packet packet)
{
    if (m_Failed) return false;
    if (m_Send(packet)) return true;
    m_Failed = true;
    m_Pending = false;
    m_Failure(); // Session teardown releases the host's remaining input state.
    return false;
}

void MacPenInput::cancel()
{
    if (m_RemoteOwned) {
        Packet packet;
        packet.action = LI_TOUCH_EVENT_CANCEL_ALL;
        packet.tool = LI_TOOL_TYPE_UNKNOWN;
        send(packet);
    }
    m_RemoteOwned = m_RemoteDown = false;
    if (m_RemoteCursor) m_RemoteCursor(false);
}

void MacPenInput::clearSample()
{
    m_Pending = m_TouchSeen = false;
    m_AxesSeen = 0;
}

void MacPenInput::suspend(bool resetLocal)
{
    m_RequireLift = m_RequireLift || (m_State & InteractionFlags) || m_RemoteDown;
    if (resetLocal) m_LocalRequiresLift = m_LocalRequiresLift || (m_State & InteractionFlags);
    clearSample(); // Never replay queued contact after focus/capture changes.
    cancel();
    if (resetLocal && m_ResetLocal) m_ResetLocal();
}

void MacPenInput::handle(const SDL_Event& event)
{
    if (m_Failed || !isPenEvent(event.type)) return;
    // SDL's pen structures share this prefix, but read the named union member.
    SDL_PenID pen;
    SDL_WindowID window;
    SDL_PenInputFlags state = 0;
    float x = 0, y = 0;
    switch (event.type) {
    case SDL_EVENT_PEN_PROXIMITY_IN:
    case SDL_EVENT_PEN_PROXIMITY_OUT:
        pen = event.pproximity.which; window = event.pproximity.windowID; break;
    case SDL_EVENT_PEN_DOWN:
    case SDL_EVENT_PEN_UP:
        pen = event.ptouch.which; window = event.ptouch.windowID;
        state = event.ptouch.pen_state; x = event.ptouch.x; y = event.ptouch.y; break;
    case SDL_EVENT_PEN_MOTION:
        pen = event.pmotion.which; window = event.pmotion.windowID;
        state = event.pmotion.pen_state; x = event.pmotion.x; y = event.pmotion.y; break;
    case SDL_EVENT_PEN_BUTTON_DOWN:
    case SDL_EVENT_PEN_BUTTON_UP:
        pen = event.pbutton.which; window = event.pbutton.windowID;
        state = event.pbutton.pen_state; x = event.pbutton.x; y = event.pbutton.y; break;
    case SDL_EVENT_PEN_AXIS:
        pen = event.paxis.which; window = event.paxis.windowID;
        state = event.paxis.pen_state; x = event.paxis.x; y = event.paxis.y; break;
    default: return;
    }
    if (!pen) return;
    if (event.type == SDL_EVENT_PEN_PROXIMITY_OUT) {
        if (pen == m_Pen) {
            flush(); cancel(); clearSample();
            if (m_ResetLocal) m_ResetLocal();
            m_Pen = 0; m_State = 0; m_RequireLift = m_LocalRequiresLift = false;
        }
        return;
    }
    const bool touch = event.type == SDL_EVENT_PEN_DOWN || event.type == SDL_EVENT_PEN_UP;
    const bool validAxis = event.type == SDL_EVENT_PEN_AXIS &&
            event.paxis.axis >= 0 && event.paxis.axis < SDL_PEN_AXIS_COUNT;
    if (m_Pending && (pen != m_Pen || window != m_Window ||
            event.common.timestamp != m_Timestamp || (touch && m_TouchSeen) ||
            (validAxis && (m_AxesSeen & (1u << event.paxis.axis))))) {
        flush();
    }
    if (pen != m_Pen) {
        cancel(); clearSample();
        if (m_ResetLocal) m_ResetLocal();
        m_Pen = pen; m_State = 0;
        m_Pressure = m_TiltX = m_TiltY = 0;
        // A different tool can start independently of a suspended old tool.
        m_RequireLift = m_LocalRequiresLift = false;
    }
    if (event.type == SDL_EVENT_PEN_PROXIMITY_IN) return; // No coordinates yet.
    if (!std::isfinite(x) || !std::isfinite(y) ||
            (event.type == SDL_EVENT_PEN_AXIS &&
             (!validAxis || !std::isfinite(event.paxis.value)))) {
        suspend();
        return;
    }
    m_Window = window; m_Timestamp = event.common.timestamp;
    m_State = state; m_X = x; m_Y = y;
    m_TouchSeen = m_TouchSeen || touch;
    m_Pending = true;
    if (validAxis) {
        m_AxesSeen |= 1u << event.paxis.axis;
        switch (event.paxis.axis) {
        case SDL_PEN_AXIS_PRESSURE: m_Pressure = std::clamp(event.paxis.value, 0.0f, 1.0f); break;
        case SDL_PEN_AXIS_XTILT: m_TiltX = std::clamp(event.paxis.value, -90.0f, 90.0f); break;
        case SDL_PEN_AXIS_YTILT: m_TiltY = std::clamp(event.paxis.value, -90.0f, 90.0f); break;
        default: break; // Barrel rotation is not the protocol's tilt azimuth.
        }
    }
}

void MacPenInput::flush()
{
    if (!m_Pending || m_Failed) return;
    clearSample();
    const bool down = (m_State & SDL_PEN_INPUT_DOWN) != 0;
    // Route complete coordinates to local controls too. Cocoa's synthetic
    // mouse-down precedes its new position and must not click a stale control.
    if (m_LocalRequiresLift && !(m_State & InteractionFlags)) m_LocalRequiresLift = false;
    if (!m_LocalRequiresLift && m_Local && m_Local(m_Pen, m_Window, m_X, m_Y, m_State, m_Timestamp)) {
        m_RequireLift = (m_State & InteractionFlags) != 0;
        cancel();
        return;
    }
    if (m_RequireLift) {
        if (m_State & InteractionFlags) return;
        m_RequireLift = false;
    }
    Packet packet;
    if (!m_Map(m_Window, m_X, m_Y, packet.x, packet.y) ||
            !std::isfinite(packet.x) || !std::isfinite(packet.y) ||
            packet.x < 0 || packet.x > 1 || packet.y < 0 || packet.y > 1) {
        m_RequireLift = (m_State & InteractionFlags) != 0;
        cancel();
        return;
    }
    packet.tool = (m_State & SDL_PEN_INPUT_ERASER_TIP) ? LI_TOOL_TYPE_ERASER : LI_TOOL_TYPE_PEN;
    if (m_RemoteOwned && m_RemoteTool != packet.tool) cancel();
    packet.action = down ? (m_RemoteDown ? LI_TOUCH_EVENT_MOVE : LI_TOUCH_EVENT_DOWN) :
                          (m_RemoteDown ? LI_TOUCH_EVENT_UP : LI_TOUCH_EVENT_HOVER);
    packet.buttons = ((m_State & SDL_PEN_INPUT_BUTTON_1) ? LI_PEN_BUTTON_PRIMARY : 0) |
                     ((m_State & SDL_PEN_INPUT_BUTTON_2) ? LI_PEN_BUTTON_SECONDARY : 0) |
                     ((m_State & SDL_PEN_INPUT_BUTTON_3) ? LI_PEN_BUTTON_TERTIARY : 0);
    packet.pressure = down ? m_Pressure : 0.0f; // Cocoa provides no hover distance.
    // Match PLANK's Linux normalized pen convention, including top-down Y.
    const double pi = std::acos(-1.0);
    const double tx = std::tan(std::clamp(double(m_TiltX), -89.9999, 89.9999) * pi / 180.0);
    const double ty = std::tan(std::clamp(double(m_TiltY), -89.9999, 89.9999) * pi / 180.0);
    packet.tilt = Uint8(std::lround(std::atan(std::hypot(tx, ty)) * 180.0 / pi));
    double direction = -std::atan2(tx, ty) * 180.0 / pi;
    if (direction < 0) direction += 360.0;
    packet.rotation = Uint16(std::lround(direction)) % 360;
    if (send(packet)) {
        m_RemoteOwned = true; m_RemoteDown = down; m_RemoteTool = packet.tool;
        // Reclaim after an intervening real mouse event too. Only accepted,
        // fully assembled remote samples may hide the native pointer.
        if (m_RemoteCursor) m_RemoteCursor(true);
    }
}
