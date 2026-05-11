#pragma once

#include "Pch.h"

class Framebuffer;





////////////////////////////////////////////////////////////////////////////////
//
//  InputEvent
//
////////////////////////////////////////////////////////////////////////////////

struct InputEvent
{
    enum class Kind
    {
        KeyDown,
        KeyUp,
        MouseMove,
        MouseButton,
        Quit,
    };

    Kind        kind;
    uint32_t    code;
    int32_t     x;
    int32_t     y;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IAudioSink
//
////////////////////////////////////////////////////////////////////////////////

class IAudioSink
{
public:
    virtual              ~IAudioSink () = default;

    virtual HRESULT       PushSamples (const float * samples, size_t count) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IHostShell
//
//  Host edge for the emulator. Production binding: Casso/HostShell.cpp
//  (Win32). Test binding: UnitTest/EmuTests/HeadlessHost.cpp (mocks the
//  window, audio device, and input pump per constitution §II — no Win32
//  resources, no audio device, no host filesystem, no registry, no
//  network).
//
////////////////////////////////////////////////////////////////////////////////

class IHostShell
{
public:
    virtual              ~IHostShell () = default;

    virtual HRESULT       OpenWindow      (int w, int h) = 0;
    virtual HRESULT       PresentFrame    (const Framebuffer & fb) = 0;
    virtual HRESULT       OpenAudioDevice (IAudioSink * & outSink) = 0;
    virtual HRESULT       PollInput       (std::vector<InputEvent> & outEvents) = 0;
};
