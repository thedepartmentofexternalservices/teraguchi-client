#include "macsystemkeys.h"
#include <map>
#include <set>
#include <utility>

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <Carbon/Carbon.h>
#include "backend/teraguchi/macinputaccess.h"

struct MacSystemKeys::Impl
{
    Window window;
    Receiver receiver;
    std::function<void()> failure;
    CFMachPortRef tap = nullptr;
    CFRunLoopSourceRef source = nullptr;
    id monitor = nil;
    Uint32 eventType = 0;
    NSUInteger nextId = 0;
    bool failed = false;
    struct Pending { SDL_KeyboardEvent key; bool queued = false; };
    std::map<NSUInteger, Pending> pending;
    std::map<CGKeyCode, SDL_Scancode> held;
    std::set<CGKeyCode> cancelled;
    static constexpr short Marker = 31037;

    Impl(Window w, Receiver r, std::function<void()> f)
        : window(std::move(w)), receiver(std::move(r)), failure(std::move(f)) {}

    void fail()
    {
        if (failed) return;
        failed = true;
        cancel();
        failure(); // Latch only; the streaming loop owns cleanup and the error.
    }

    void cancel()
    {
        for (const auto& key : held) cancelled.insert(key.first);
        held.clear();
        pending.clear(); // Invalidate both AppKit and SDL markers, without raw pointers.
    }

    static SDL_Scancode chord(CGKeyCode code, CGEventFlags flags)
    {
        // Keep the OS Force Quit escape hatch available even during capture.
        if (code == kVK_Escape && (flags & kCGEventFlagMaskCommand) &&
                (flags & kCGEventFlagMaskAlternate)) return SDL_SCANCODE_UNKNOWN;
        if (flags & kCGEventFlagMaskCommand) {
            switch (code) {
            case kVK_Tab: return SDL_SCANCODE_TAB;
            case kVK_Space: return SDL_SCANCODE_SPACE;
            case kVK_ANSI_Grave: return SDL_SCANCODE_GRAVE;
            case kVK_ANSI_Q: return SDL_SCANCODE_Q;
            case kVK_ANSI_W: return SDL_SCANCODE_W;
            case kVK_ANSI_H: return SDL_SCANCODE_H;
            case kVK_ANSI_M: return SDL_SCANCODE_M;
            case kVK_Escape: return SDL_SCANCODE_ESCAPE;
            }
        }
        if (flags & kCGEventFlagMaskControl) {
            switch (code) {
            case kVK_LeftArrow: return SDL_SCANCODE_LEFT;
            case kVK_UpArrow: return SDL_SCANCODE_UP;
            case kVK_RightArrow: return SDL_SCANCODE_RIGHT;
            case kVK_DownArrow: return SDL_SCANCODE_DOWN;
            }
        }
        return SDL_SCANCODE_UNKNOWN;
    }

    bool prepareDelivery()
    {
        if (monitor) return true;
        eventType = SDL_RegisterEvents(1);
        // PLANK already multiplexes its control messages on SDL_EVENT_USER.
        // SDL's allocator doesn't know about that legacy reservation.
        if (eventType == SDL_EVENT_USER) eventType = SDL_RegisterEvents(1);
        if (!eventType) return false;
        monitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskApplicationDefined
            handler:^NSEvent*(NSEvent* event) {
                if (event.subtype != Marker || event.data1 != static_cast<NSInteger>(eventType)) return event;
                const auto it = pending.find(static_cast<NSUInteger>(event.data2));
                if (it != pending.end() && !it->second.queued) {
                    SDL_Event marker{};
                    marker.type = eventType;
                    marker.user.windowID = it->second.key.windowID;
                    marker.user.data1 = reinterpret_cast<void*>(static_cast<uintptr_t>(it->first));
                    it->second.queued = true;
                    if (!SDL_PushEvent(&marker)) fail();
                }
                return nil;
            }];
        return monitor != nil;
    }

    void enqueue(CGKeyCode code, SDL_Scancode scancode, bool down, CGEventFlags flags,
                 SDL_WindowID target)
    {
        if (pending.size() >= 128 || ++nextId == 0) { fail(); return; }
        SDL_KeyboardEvent key{};
        key.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        key.timestamp = SDL_GetTicksNS();
        key.windowID = target;
        key.scancode = scancode;
        key.raw = code;
        key.down = down;
        if (flags & kCGEventFlagMaskCommand) key.mod |= SDL_KMOD_GUI;
        if (flags & kCGEventFlagMaskControl) key.mod |= SDL_KMOD_CTRL;
        if (flags & kCGEventFlagMaskAlternate) key.mod |= SDL_KMOD_ALT;
        if (flags & kCGEventFlagMaskShift) key.mod |= SDL_KMOD_SHIFT;
        key.key = SDL_GetKeyFromScancode(scancode, key.mod, true);
        pending.emplace(nextId, Pending{key});
        // Never push directly from the tap: prior Cocoa pen/modifier events may
        // still be awaiting SDL translation. Tail-post an inert native marker.
        NSEvent* marker = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
            location:NSZeroPoint modifierFlags:0 timestamp:[NSProcessInfo processInfo].systemUptime
            windowNumber:0 context:nil subtype:Marker data1:eventType data2:nextId];
        [NSApp postEvent:marker atStart:NO];
    }

    CGEventRef accept(CGEventType type, CGEventRef event)
    {
        if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
            fail(); // Don't silently lose capture or override an OS/user disable.
            return event;
        }
        if (type != kCGEventKeyDown && type != kCGEventKeyUp) return event;
        const auto code = static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
        const bool down = type == kCGEventKeyDown;
        const bool repeat = CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0;
        const SDL_WindowID target = failed ? 0 : window();
        if (!target) cancel();
        const auto old = held.find(code);
        if (old != held.end()) {
            if (!down) {
                enqueue(code, old->second, false, CGEventGetFlags(event), target);
                held.erase(code);
            }
            return nullptr; // The original down owns its up, even after Command lifts.
        }
        if (cancelled.count(code)) {
            if (!down) cancelled.erase(code);
            return nullptr; // Require a physical release after interruption.
        }
        if (!target || !down || repeat) return event;
        const auto scancode = chord(code, CGEventGetFlags(event));
        if (scancode == SDL_SCANCODE_UNKNOWN) return event;
        held.emplace(code, scancode);
        enqueue(code, scancode, true, CGEventGetFlags(event), target);
        return nullptr;
    }

    static CGEventRef callback(CGEventTapProxy, CGEventType type, CGEventRef event, void* context)
    {
        return static_cast<Impl*>(context)->accept(type, event);
    }

    bool start()
    {
        SDL_assert(SDL_IsMainThread());
        if (failed) return false;
        if (tap) return CGEventTapIsEnabled(tap);
        if (!MacInputAccess::query().ready() || !prepareDelivery()) return false;
        const CGEventMask mask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp);
        tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
                              kCGEventTapOptionDefault, mask, callback, this);
        if (!tap) return false;
        source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
        if (!source) return false;
        CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);
        CGEventTapEnable(tap, true);
        return CGEventTapIsEnabled(tap);
    }

    bool dispatch(const SDL_Event& event)
    {
        if (event.type != eventType || !eventType) return false;
        const auto it = pending.find(static_cast<NSUInteger>(reinterpret_cast<uintptr_t>(event.user.data1)));
        if (it == pending.end()) return true;
        const auto key = it->second;
        pending.erase(it);
        if (!failed && key.queued && window() == key.key.windowID) {
            auto delivered = key.key;
            receiver(delivered);
        }
        return true;
    }

    ~Impl()
    {
        SDL_assert(SDL_IsMainThread());
        if (tap) CGEventTapEnable(tap, false);
        if (source) {
            CFRunLoopRemoveSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);
            CFRelease(source);
        }
        if (tap) { CFMachPortInvalidate(tap); CFRelease(tap); }
        if (monitor) [NSEvent removeMonitor:monitor];
    }
};

MacSystemKeys::MacSystemKeys(Window window, Receiver receiver, std::function<void()> failure)
    : m_Impl(std::make_unique<Impl>(std::move(window), std::move(receiver), std::move(failure))) {}
MacSystemKeys::~MacSystemKeys() = default;
bool MacSystemKeys::start() { return m_Impl->start(); }
void MacSystemKeys::cancel() { m_Impl->cancel(); }
bool MacSystemKeys::dispatch(const SDL_Event& event) { return m_Impl->dispatch(event); }
