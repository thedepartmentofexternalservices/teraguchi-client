#pragma once

#include <Limelight.h>
#include <SDL3/SDL.h>
#include <functional>

// SDL's Cocoa backend splits a single NSEvent into touch, motion, buttons and
// axes with one timestamp. Preserve that sample until its final axis arrives.
class MacPenInput
{
public:
    struct Packet {
        Uint8 action = LI_TOUCH_EVENT_HOVER;
        Uint8 tool = LI_TOOL_TYPE_PEN;
        Uint8 buttons = 0;
        float x = 0, y = 0, pressure = 0;
        Uint16 rotation = 0;
        Uint8 tilt = 0;
    };
    using Sender = std::function<bool(const Packet&)>;
    using Mapper = std::function<bool(SDL_WindowID, float, float, float&, float&)>;
    using LocalHandler = std::function<bool(SDL_PenID, SDL_WindowID, float, float, SDL_PenInputFlags, Uint64)>;

    MacPenInput(Sender sender, Mapper mapper, std::function<void()> failure,
                LocalHandler local = {}, std::function<void()> resetLocal = {},
                std::function<void(bool)> remoteCursor = {});
    static bool isPenEvent(Uint32 type);
    static bool isSyntheticMouse(const SDL_Event& event);
    void beforeEvent(const SDL_Event& event);
    void handle(const SDL_Event& event);
    void flush();
    void suspend(bool resetLocal = true);
    bool pending() const { return m_Pending; }

private:
    bool send(Packet packet);
    void cancel();
    void clearSample();
    Sender m_Send;
    Mapper m_Map;
    std::function<void()> m_Failure;
    LocalHandler m_Local;
    std::function<void()> m_ResetLocal;
    std::function<void(bool)> m_RemoteCursor;
    SDL_PenID m_Pen = 0;
    SDL_WindowID m_Window = 0;
    Uint64 m_Timestamp = 0;
    SDL_PenInputFlags m_State = 0;
    Uint32 m_AxesSeen = 0;
    float m_X = 0, m_Y = 0, m_Pressure = 0, m_TiltX = 0, m_TiltY = 0;
    bool m_Pending = false, m_TouchSeen = false, m_RequireLift = false;
    bool m_LocalRequiresLift = false;
    bool m_RemoteOwned = false, m_RemoteDown = false, m_Failed = false;
    Uint8 m_RemoteTool = LI_TOOL_TYPE_PEN;
};
