#include "streaming/session.h"

#include <Limelight.h>
#include <SDL3/SDL.h>
#include "keyboardmap.h"
#ifdef Q_OS_MACOS
#include "mackeyboard.h"
#include "macsystemkeys.h"

#include "streaming/macclipboardsync.h"

SDL_WindowID SdlInputHandler::focusedKeyboardWindow() const
{
    for (const auto& output : m_PresentationLayout.outputs) {
        if (SDL_GetWindowFlags(output.window) & SDL_WINDOW_INPUT_FOCUS)
            return SDL_GetWindowID(output.window);
    }
    return 0;
}

void SdlInputHandler::initializeMacKeyboard()
{
    raiseAllKeys();
    m_MacKeyboard = std::make_unique<MacKeyboardState>(
        [](const MacKeyboardState::Packet& packet) {
            return LiSendKeyboardEvent2(packet.key, packet.down ? KEY_ACTION_DOWN : KEY_ACTION_UP,
                                        packet.modifiers, packet.flags) == 0;
        }, [] { Session::get()->rejectKeyboardInput(false); });
    m_MacSystemKeys = std::make_unique<MacSystemKeys>(
        [this] { return isCaptureActive() && isSystemKeyCaptureActive() ? focusedKeyboardWindow() : 0; },
        [this](SDL_KeyboardEvent& event) { handleKeyEvent(&event); },
        [] { Session::get()->rejectKeyboardInput(false); });
}

bool SdlInputHandler::dispatchMacSystemKey(const SDL_Event& event)
{
    return m_MacSystemKeys && m_MacSystemKeys->dispatch(event);
}
#endif

void SdlInputHandler::performSpecialKeyCombo(KeyCombo combo)
{
    switch (combo) {
    case KeyComboQuit:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected quit key combo");

        // Push a quit event to the main loop
        SDL_Event event;
        event.type = SDL_EVENT_QUIT;
        event.quit.timestamp = SDL_GetTicks();
        SDL_PushEvent(&event);
        break;

    case KeyComboUngrabInput:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected mouse capture toggle combo");

        // Stop handling future input
        setCaptureActive(!isCaptureActive());

        // Force raise all keys to ensure they aren't stuck,
        // since we won't get their key up events.
        raiseAllKeys();
        break;

    case KeyComboToggleFullScreen:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected full-screen toggle combo");
        Session::s_ActiveSession->toggleFullscreen();

        // Force raise all keys just be safe across this full-screen/windowed
        // transition just in case key events get lost.
        raiseAllKeys();
        break;

    case KeyComboToggleStatsOverlay:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected stats toggle combo");

        // Toggle the stats overlay
        Session::get()->getOverlayManager().setOverlayState(Overlay::OverlayDebug,
                                                            !Session::get()->getOverlayManager().isOverlayEnabled(Overlay::OverlayDebug));
        break;

    case KeyComboToggleMinimize:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected minimize combo");
        SDL_MinimizeWindow(m_Window);
        break;

    case KeyComboPasteText:
    {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected type clipboard text combo");

        // Force raise all keys to ensure that none of them interfere
        // with the text we're going to type.
        raiseAllKeys();

        char* text = nullptr;
#ifdef Q_OS_MACOS
        text = macReadGeneralPasteboardTextForSdl();
#endif
        if (text == nullptr && SDL_HasClipboardText()) {
            text = SDL_GetClipboardText();
        }
        if (text != nullptr) {
            // Sending both CR and LF will lead to two newlines in the destination for
            // each newline in the source, so we fix up any CRLFs into just a single LF.
            for (char* c = text; *c != 0; c++) {
                if (*c == '\r' && *(c + 1) == '\n') {
                    // We're using strlen() rather than strlen() - 1 since we need to add 1
                    // to copy the null terminator which is not included in strlen()'s count.
                    memmove(c, c + 1, strlen(c));
                }
            }

            // Send this text to the PC
            LiSendUtf8TextEvent(text, (unsigned int)strlen(text));

            // SDL_GetClipboardText() allocates, so we must free
            SDL_free((void*)text);
        }
        else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "No text in clipboard to paste!");
        }
        break;
    }

    case KeyComboTogglePointerRegionLock:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected pointer region lock toggle combo");
        m_PointerRegionLockActive = !m_PointerRegionLockActive;

        // Remember that the user changed this manually, so we don't mess with it anymore
        // during windowed <-> full-screen transitions.
        m_PointerRegionLockToggledByUser = true;

        // Apply the new region lock
        updatePointerRegionLock();
        break;

    case KeyComboToggleKeyboardGrab:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected keyboard grab toggle combo");

        // Toggle the system key capture mode
        if (isSystemKeyCaptureActive()) {
            m_CaptureSystemKeysMode = StreamingPreferences::CSK_OFF;
        }
        else {
            m_CaptureSystemKeysMode = StreamingPreferences::CSK_ALWAYS;
        }

        updateKeyboardGrabState();
        break;

    default:
        Q_UNREACHABLE();
    }
}

void SdlInputHandler::handleKeyEvent(SDL_KeyboardEvent* event)
{
    short keyCode;
#ifndef Q_OS_MACOS
    char modifiers;
#endif
    bool shouldNotConvertToScanCodeOnServer = false;
#ifdef Q_OS_MACOS
    const bool focused = event->windowID != 0 && event->windowID == focusedKeyboardWindow();
    if (!m_MacKeyboard) return;
#endif

    if (event->repeat) {
        // Ignore repeat key down events
        SDL_assert(event->down);
        return;
    }

    // Check for our special key combos
    if ((event->down) &&
#ifdef Q_OS_MACOS
            focused &&
#endif
            (event->mod & SDL_KMOD_CTRL) &&
            (event->mod & SDL_KMOD_ALT) &&
            (event->mod & SDL_KMOD_SHIFT)) {
        // First we test the SDLK combos for matches,
        // that way we ensure that latin keyboard users
        // can match to the key they see on their keyboards.
        // If nothing matches that, we'll then go on to
        // checking scancodes so non-latin keyboard users
        // can have working hotkeys (though possibly in
        // odd positions). We must do all SDLK tests before
        // any scancode tests to avoid issues in cases
        // where the SDLK for one shortcut collides with
        // the scancode of another.

        for (int i = 0; i < KeyComboMax; i++) {
            if (m_SpecialKeyCombos[i].enabled && event->key == m_SpecialKeyCombos[i].keyCode) {
#ifdef Q_OS_MACOS
                m_MacKeyboard->consumeLocal(event->scancode);
#endif
                performSpecialKeyCombo(m_SpecialKeyCombos[i].keyCombo);
                return;
            }
        }

        for (int i = 0; i < KeyComboMax; i++) {
            if (m_SpecialKeyCombos[i].enabled && event->scancode == m_SpecialKeyCombos[i].scanCode) {
#ifdef Q_OS_MACOS
                m_MacKeyboard->consumeLocal(event->scancode);
#endif
                performSpecialKeyCombo(m_SpecialKeyCombos[i].keyCombo);
                return;
            }
        }
    }

#ifndef Q_OS_MACOS
    // Set modifier flags
    modifiers = 0;
    if (event->mod & SDL_KMOD_CTRL) {
        modifiers |= MODIFIER_CTRL;
    }
    if (event->mod & SDL_KMOD_ALT) {
        modifiers |= MODIFIER_ALT;
    }
    if (event->mod & SDL_KMOD_SHIFT) {
        modifiers |= MODIFIER_SHIFT;
    }
    if (event->mod & SDL_KMOD_GUI) {
        if (isSystemKeyCaptureActive()) {
            modifiers |= MODIFIER_META;
        }
    }

#endif

#ifndef Q_OS_MACOS
    if ((event->scancode == SDL_SCANCODE_LGUI || event->scancode == SDL_SCANCODE_RGUI) &&
            !isSystemKeyCaptureActive()) return;
#endif
    const auto mapped = PlankKeyboardMap::map(event->scancode);
    if (mapped.code < 0) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Unhandled button event: %d", event->scancode);
        return;
    }
    keyCode = static_cast<short>(mapped.code);
    shouldNotConvertToScanCodeOnServer = mapped.nonNormalized;

#ifdef Q_OS_MACOS
    const bool gui = event->scancode == SDL_SCANCODE_LGUI || event->scancode == SDL_SCANCODE_RGUI;
    m_MacKeyboard->key(event->scancode, 0x8000 | keyCode,
                      shouldNotConvertToScanCodeOnServer ? SS_KBE_FLAG_NON_NORMALIZED : 0,
                      event->down, event->repeat,
                      focused && isCaptureActive() && (!gui || isSystemKeyCaptureActive()));
#else
    // Track the key state so we always know which keys are down
    if (event->down) {
        m_KeysDown.insert(keyCode);
    }
    else {
        m_KeysDown.remove(keyCode);
    }

    LiSendKeyboardEvent2(0x8000 | keyCode,
                        event->down ?
                            KEY_ACTION_DOWN : KEY_ACTION_UP,
                        modifiers,
                        shouldNotConvertToScanCodeOnServer ? SS_KBE_FLAG_NON_NORMALIZED : 0);
#endif
}
