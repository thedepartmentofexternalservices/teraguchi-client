#pragma once

#include <SDL3/SDL.h>
#include <functional>
#include <memory>

// A tap captures only reserved chords. AppKit markers order them behind prior
// native events; the streaming loop alone forwards them through normal input.
// No key text or event traces are retained. Main-thread lifetime only.
class MacSystemKeys
{
public:
    using Window = std::function<SDL_WindowID()>;
    using Receiver = std::function<void(SDL_KeyboardEvent&)>;
    MacSystemKeys(Window window, Receiver receiver, std::function<void()> failure);
    ~MacSystemKeys();
    bool start(); // Never prompts or changes permissions. False is a capture failure.
    void cancel();
    bool dispatch(const SDL_Event& event);
    struct Impl;
private:
    std::unique_ptr<Impl> m_Impl;
};
