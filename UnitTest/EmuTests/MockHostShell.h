#pragma once

#include "../../CassoEmuCore/Pch.h"

#include "Core/IHostShell.h"
#include "MockAudioSink.h"

class Framebuffer;





////////////////////////////////////////////////////////////////////////////////
//
//  MockHostShell
//
//  IHostShell test double. Constitution §II compliant — never opens a
//  Win32 window, never opens an audio device, never reads host files,
//  never touches the registry / network / environment.
//
//  OpenWindow records the requested dimensions; PresentFrame remembers
//  the last frame was presented; OpenAudioDevice hands out an embedded
//  MockAudioSink; PollInput drains an internally-injected event queue.
//
////////////////////////////////////////////////////////////////////////////////

class MockHostShell : public IHostShell
{
public:
                        MockHostShell ();

    HRESULT             OpenWindow      (int w, int h) override;
    HRESULT             PresentFrame    (const Framebuffer & fb) override;
    HRESULT             OpenAudioDevice (IAudioSink * & outSink) override;
    HRESULT             PollInput       (std::vector<InputEvent> & outEvents) override;

    void                InjectEvent (const InputEvent & evt);

    int                 GetWindowWidth     () const { return m_windowWidth; }
    int                 GetWindowHeight    () const { return m_windowHeight; }
    bool                WasWindowOpened    () const { return m_windowOpened; }
    uint64_t            GetPresentCount    () const { return m_presentCount; }
    MockAudioSink &     GetAudioSink       () { return m_audioSink; }

private:
    MockAudioSink             m_audioSink;
    std::vector<InputEvent>   m_pendingEvents;
    int                       m_windowWidth   = 0;
    int                       m_windowHeight  = 0;
    bool                      m_windowOpened  = false;
    uint64_t                  m_presentCount  = 0;
};
