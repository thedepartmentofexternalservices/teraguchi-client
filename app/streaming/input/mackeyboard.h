#pragma once

#include <SDL3/SDL.h>
#include <cstdint>
#include <functional>
#include <set>
#include <vector>

// Physical press ownership, independent of keymap/layout changes at key-up.
// All calls belong to the streaming event thread, including cancellation.
class MacKeyboardState
{
public:
    struct Packet {
        std::uint16_t key;
        bool down;
        std::uint8_t modifiers;
        std::uint8_t flags;
    };
    using Sender = std::function<bool(const Packet&)>;
    MacKeyboardState(Sender sender, std::function<void()> failure);
    void key(SDL_Scancode physical, std::uint16_t key, std::uint8_t flags,
             bool down, bool repeat, bool allowPress);
    void consumeLocal(SDL_Scancode physical);
    void releaseAll();

private:
    struct Held { SDL_Scancode physical; std::uint16_t key; std::uint8_t flags; };
    std::uint8_t modifiers() const;
    bool send(Packet packet);
    void release(SDL_Scancode physical);
    Sender m_Send;
    std::function<void()> m_Failure;
    std::vector<Held> m_Held;
    std::set<SDL_Scancode> m_Suppressed;
    bool m_Failed = false;
};
