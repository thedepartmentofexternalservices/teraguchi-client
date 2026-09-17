#include "mackeyboard.h"
#include <Limelight.h>
#include <algorithm>
#include <utility>

MacKeyboardState::MacKeyboardState(Sender sender, std::function<void()> failure)
    : m_Send(std::move(sender)), m_Failure(std::move(failure)) {}

std::uint8_t MacKeyboardState::modifiers() const
{
    std::uint8_t result = 0;
    for (const auto& held : m_Held) {
        switch (held.key & 0xff) {
        case 0xA0: case 0xA1: result |= MODIFIER_SHIFT; break;
        case 0xA2: case 0xA3: result |= MODIFIER_CTRL; break;
        case 0xA4: case 0xA5: result |= MODIFIER_ALT; break;
        case 0x5B: case 0x5C: result |= MODIFIER_META; break;
        }
    }
    return result;
}

bool MacKeyboardState::send(Packet packet)
{
    if (m_Failed) return false;
    if (m_Send(packet)) return true;
    m_Failed = true;
    m_Failure();
    return false;
}

void MacKeyboardState::release(SDL_Scancode physical)
{
    const auto it = std::find_if(m_Held.begin(), m_Held.end(),
                                [physical](const Held& h) { return h.physical == physical; });
    if (it == m_Held.end()) return;
    const Held held = *it;
    m_Held.erase(it);
    // The legacy map aliases keypad Enter and Return. Until a negotiated host
    // distinction exists, don't release one while its other physical key holds.
    const bool aliasHeld = std::any_of(m_Held.begin(), m_Held.end(), [&](const Held& h) {
        return h.key == held.key && h.flags == held.flags;
    });
    if (!aliasHeld) send({held.key, false, modifiers(), held.flags});
}

void MacKeyboardState::key(SDL_Scancode physical, std::uint16_t key, std::uint8_t flags,
                           bool down, bool repeat, bool allowPress)
{
    if (physical == SDL_SCANCODE_UNKNOWN || m_Failed) return;
    if (!down) {
        m_Suppressed.erase(physical);
        release(physical);
        return;
    }
    if (repeat || m_Suppressed.count(physical) ||
            std::any_of(m_Held.begin(), m_Held.end(),
                        [physical](const Held& h) { return h.physical == physical; })) return;
    if (!allowPress) {
        m_Suppressed.insert(physical);
        return;
    }
    const bool aliasHeld = std::any_of(m_Held.begin(), m_Held.end(), [&](const Held& h) {
        return h.key == key && h.flags == flags;
    });
    m_Held.push_back({physical, key, flags});
    if (!aliasHeld) send({key, true, modifiers(), flags});
}

void MacKeyboardState::consumeLocal(SDL_Scancode physical)
{
    release(physical);
    m_Suppressed.insert(physical);
}

void MacKeyboardState::releaseAll()
{
    while (!m_Held.empty()) {
        const auto physical = m_Held.back().physical;
        m_Suppressed.insert(physical);
        release(physical);
    }
}
