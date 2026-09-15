#include "macpresentationwindows.h"
#import <AppKit/AppKit.h>

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
