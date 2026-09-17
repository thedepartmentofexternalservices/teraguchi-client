#include "macpresentationwindows.h"
#import <AppKit/AppKit.h>
#include <SDL3/SDL_log.h>

void MacPresentationWindows::logDisplaySpacePolicy()
{
    if (!NSApp) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "PLANK macOS display spaces: AppKit unavailable");
        return;
    }
    const bool separateSpaces = [NSScreen screensHaveSeparateSpaces];
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "PLANK macOS display spaces: separate=%s presentation=borderless-desktop",
                separateSpaces ? "yes" : "no");
}

MacPresentationWindows::SystemUiScope::~SystemUiScope()
{
    setActive(false);
}

bool MacPresentationWindows::SystemUiScope::setActive(bool active)
{
    if (!NSApp) return !active;
    if (!active && !m_Active) return true;
    if (active && !m_Active) m_PreviousOptions = NSApp.presentationOptions;
    // Auto-hide still intercepts the screen edge. Keep it available to Linux
    // while this pair has focus, without disabling app switching or Force Quit.
    const auto options = active ?
        (m_PreviousOptions & ~(NSApplicationPresentationAutoHideDock |
                              NSApplicationPresentationAutoHideMenuBar |
                              NSApplicationPresentationAutoHideToolbar)) |
            NSApplicationPresentationHideDock | NSApplicationPresentationHideMenuBar :
        m_PreviousOptions;
    if (NSApp.presentationOptions != options) NSApp.presentationOptions = options;
    m_Active = active;
    return NSApp.presentationOptions == options;
}

bool MacPresentationWindows::configure(SDL_Window* window)
{
    if (!window) return false;
    auto* native = (__bridge NSWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                                              SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    if (!native) return false;
    // The toolbar owns the paired fullscreen transition. macOS's green button
    // would otherwise put just this window into its own fullscreen Space.
    native.collectionBehavior = (native.collectionBehavior &
        ~(NSWindowCollectionBehaviorFullScreenPrimary | NSWindowCollectionBehaviorFullScreenAuxiliary)) |
        NSWindowCollectionBehaviorFullScreenNone;
    [native standardWindowButton:NSWindowZoomButton].enabled = NO;
    return true;
}
