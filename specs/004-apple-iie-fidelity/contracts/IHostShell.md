# Contract: IHostShell

Host edge for the emulator. Production binding: `Casso/HostShell.cpp` (Win32).
Test binding: `UnitTest/EmuTests/HeadlessHost.cpp`.

## Header

```cpp
struct InputEvent {
    enum class Kind { KeyDown, KeyUp, MouseMove, MouseButton, Quit };
    Kind     kind;
    uint32_t code;     // virtual-key for KeyDown/Up; button id for MouseButton
    int32_t  x, y;     // mouse
};

class IAudioSink {
public:
    virtual         ~IAudioSink () = default;
    virtual HRESULT  PushSamples (const float * samples, size_t count) = 0;
};

class IHostShell {
public:
    virtual         ~IHostShell () = default;
    virtual HRESULT  OpenWindow      (int w, int h) = 0;
    virtual HRESULT  PresentFrame    (const Framebuffer & fb) = 0;
    virtual HRESULT  OpenAudioDevice (IAudioSink * & outSink) = 0;
    virtual HRESULT  PollInput       (std::vector<InputEvent> & outEvents) = 0;
};
```

## Production semantics (`Casso/HostShell.cpp`)
- `OpenWindow` → CreateWindowExW.
- `PresentFrame` → blit (StretchDIBits or D3D depending on existing path).
- `OpenAudioDevice` → returns a WASAPI sink.
- `PollInput` → drains the Win32 message queue, translates to InputEvents.

## Test semantics (`HeadlessHost`)
- `OpenWindow` → no-op; record requested dimensions.
- `PresentFrame` → copies framebuffer to a `LastFrame` cache for hashing.
- `OpenAudioDevice` → returns a `MockAudioSink` that drops samples + counts toggles.
- `PollInput` → drains an internally-injected event queue.
- Adds `InjectKey(KeyDown/Up, code)`, `RunFor(cycles)`, `RunUntil(predicate, maxCycles)`.

## Test isolation contract (NON-NEGOTIABLE)

The headless implementation MUST NOT:
- create a Win32 window
- open an audio device
- read any host file (use `IFixtureProvider` for fixtures)
- access the registry, network, or environment

Verified by code review and by the test-runner sandbox check.
