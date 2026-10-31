#pragma once

#include "Pch.h"

#include "Capture/ScreenshotMetadata.h"
#include "Config/CrtTypes.h"
#include "D3DRenderer.h"
#include "Shell/FrameClock.h"
#include "Shell/ScreenshotCapture.h"
#include "Ui/ColorUtil.h"
#include "Ui/UiCommandTypes.h"



class EmulatorShell;
struct MonitorSpec;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellRenderer
//
//  The picture: the framebuffer renderer that composites into the host's
//  back buffer, the CPU and UI framebuffers and the lock between them, the
//  CPU thread's render and publish with its render-skip gate and frame
//  clock, the color mode and CRT settings the picture is drawn with, the UI
//  thread's present, and screenshots.
//
//  The CPU thread's frame loop and the shell's window handlers still reach
//  the framebuffers and the gate directly, so the shell is a friend.
//
////////////////////////////////////////////////////////////////////////////////

class ShellRenderer
{
    friend class EmulatorShell;

public:
    explicit ShellRenderer (EmulatorShell & shell);
    ~ShellRenderer();

    // The framebuffer renderer, for the chrome, the scene and the drives that
    // draw into the same frame.
    D3DRenderer &  GetD3D () { return m_d3dRenderer; }

    // The color mode the CPU thread renders with. The UI thread sets it; the
    // CPU thread picks it up on its next frame.
    ColorMode  GetColorMode () const          { return m_colorMode.load (std::memory_order_acquire); }
    void       SetColorMode (ColorMode mode)  { m_colorMode.store (mode, std::memory_order_release); }

    // The CRT override key for one color mode of the monitor on the desk.
    const std::string &  GetCrtOverrideKey (size_t mode) const { return m_crtOverrideKeys[mode]; }

    // Runs ONE UI-thread render cycle: latch the newest emulator framebuffer,
    // push CRT params, advance chrome / panel animation, refresh the printer
    // indicator + live preview (which also paces the printer audio), and -- if
    // anything needs presenting -- drive a synchronous WM_PAINT. Returns true iff
    // it presented (caller idle-sleeps when false). Factored out of RunMessageLoop
    // so the host's OnModalLoopTick can pump it while the OS modal move / size
    // loop owns the thread (otherwise the preview + sound freeze on a title-bar
    // hold, then jump on release). The host owns the keep-alive timer; the shell
    // only supplies this per-frame work.
    bool TryPresentUiFrame();

    void RenderFramebuffer();

    // Take a screenshot in the user's configured mode: copy it to the
    // clipboard, write the PNG if saving is on, and say what happened.
    // Bound to the toolbar camera, Edit > Copy screenshot, and Ctrl+Alt+C.
    void TakeScreenshot();

    // Presentation pacing + render-skip gate (rationale in the .cpp).
    // ShouldPublishFrame throttles rasterize/publish to ~60 Hz at Maximum
    // speed; the Compute* signatures feed the dirty-tracked render gate that
    // skips re-rasterizing an unchanged screen (video RAM dirty + mode +
    // flash phase + color).
    bool      ShouldPublishFrame  ();
    uint32_t  ComputeVideoModeSig ();
    bool      ComputeFlashOn      ();
    uint64_t  ComputeColorSig     ();

    void PublishFramebuffer();

    void    AllocateFramebuffers            ();

    HRESULT InitializeRenderer              ();

    // The monitor this machine ships with, from its config rather than from
    // its name. Both the desk scene's mesh and the screen's default color
    // come from the one answer, so they cannot disagree about what is
    // standing on the desk.
    const MonitorSpec &  ResolveMonitorForCurrentMachine();

    // The four override keys for the monitor currently on the desk, one per
    // color mode. Cached because resolving the monitor re-reads and re-parses
    // the machine JSON, and the render path needs a key every frame.
    // Refreshed only when the machine changes, on the UI thread.
    void          RefreshCrtOverrideKeys ();
    CrtResolved   ResolveCrtForCurrentMode () const;

    // Accessor used by the Settings → Theme preview to copy the live
    // emulator framebuffer into the mock window. The UI framebuffer is
    // the post-CRT-effects pixel buffer the chrome composes on top of;
    // returning a raw pointer is safe because the chrome composition
    // pass runs synchronously after the framebuffer is published.
    const uint32_t *  GetUiFramebufferPixels () const
    {
        return m_uiFramebuffer.empty() ? nullptr : m_uiFramebuffer.data();
    }

    // Live channel for the Settings → Display monitor dropdown. The
    // dropdown calls this on every selection so the user sees the
    // color-treatment change as they hover/select; Cancel restores
    // the baseline by calling this again with the entry-state value.
    // Bypasses the IDM command queue so the change is visible on the
    // next CPU frame rather than waiting for queue drain.
    void  SetColorModeLive (int settingsColorModeIndex);

    // Live-set the text color used on the Color monitor (0xAARRGGBB),
    // resolved from a ColorMonitorTextMode + custom color. Like
    // SetColorModeLive, the Settings panel calls this on hover / select so
    // the change shows on the next CPU frame, and on Cancel to restore.
    void  SetColorMonitorTextArgbLive (uint32_t argb);



private:
    EmulatorShell  & m_shell;

    D3DRenderer            m_d3dRenderer;

    // Per-frame framebuffer pointer staged by RunMessageLoop and read
    // by the host's before-present hook (DxuiHwndSource::PaintPump ->
    // D3DRenderer::UploadAndComposite). Points into m_uiFramebuffer
    // when the emulator produced a new frame this iteration, or nullptr
    // to re-composite the last upload (chrome-only repaints). Touched
    // only on the UI thread.
    const uint32_t *                 m_pendingFramebuffer = nullptr;

    // The padlock each drive last showed, 2D widget or 3D drive. Write
    // protection moves no pixel the machine owns, so the frame that shows it
    // has to be asked for; see the guard in the present path.
    std::array<bool, 2>         m_driveWpShown = {};

    // The info icon each drive last showed. The same reasoning: a machine
    // switch or a mount can bring it or take it away with no other pixel
    // changing.
    std::array<bool, 2>         m_driveInfoShown = {};

    //  A SCREENSHOT IN FLIGHT.
    //
    //  Scene and Crt captures cannot be taken from outside a frame: the swap
    //  chain is FLIP_DISCARD, so a presented back buffer holds nothing, and
    //  the two modes want different moments anyway -- Crt after the CRT
    //  composite but before the chrome walk, Scene after it. So TakeScreenshot
    //  arms this, drives one synchronous paint, and the paint hooks fill in
    //  the pixels at whichever point the plan asked for.
    //
    //  It is not a queue and does not survive the paint: if the frame did not
    //  service it, `captured` stays false and the capture is reported as
    //  having failed rather than silently landing on a later frame.
    //  WHERE IN THE FRAME A CAPTURE IS TAKEN. Not derivable from the source:
    //  both points can read the back buffer, and which is right depends on
    //  the mode rather than on the texture.
    //
    //  AfterPicture   end of the before-present hook. The CRT composite is
    //                 finished and nothing has painted over it -- what Crt
    //                 wants.
    //  AfterChrome    the after-paint hook. Everything the scene contributes
    //                 is in, INCLUDING the 3D drives, which render there
    //                 rather than in the composite when the monitor is off --
    //                 which is why Scene cannot simply share the point above.
    enum class CapturePoint
    {
        AfterPicture,
        AfterChrome,
    };

    struct PendingCapture
    {
        bool            armed    = false;
        bool            captured = false;
        CapturePoint    at       = CapturePoint::AfterChrome;
        CaptureSource   from     = CaptureSource::BackBufferRegion;
        RECT            regionPx = {};
        CapturedImage   image;
    };

    PendingCapture             m_pendingCapture;

    // Called from the two paint hooks at their own points in the frame; fills
    // the pending capture when the point matches what the plan asked for.
    void  ServiceCaptureRequest (CapturePoint atPoint);

    // Gathers what a screenshot can say about itself. Collecting only; which
    // entries a mode emits is decided by the composer in core.
    ScreenshotFacts  BuildScreenshotFacts (ScreenshotMode mode, const SYSTEMTIME & when) const;

    // Atomic flags (UI writes, CPU reads)
    atomic<ColorMode>             m_colorMode{ColorMode::Color};

    // Joined "<monitorConfigName>/<mode>" keys, indexed by color mode. All
    // four are cached rather than just the active one, so a mode change needs
    // no invalidation and the per-frame lookup allocates nothing.
    std::array<std::string, kCrtModeCount>  m_crtOverrideKeys;

    // Resolved text color (0xAARRGGBB) for the Color monitor. UI writes via
    // SetColorMonitorTextArgbLive; RenderFramebuffer reads it when the Color
    // monitor is active. Defaults to white.
    atomic<uint32_t>              m_colorMonitorTextArgb{ColorUtil::kWhiteArgb};

    // Double framebuffer (CPU renders, UI presents, protected by m_framebufferMutex)
    mutex                         m_framebufferMutex;
    vector<uint32_t>              m_cpuFramebuffer;
    vector<uint32_t>              m_textOverlay;
    vector<uint32_t>              m_uiFramebuffer;
    bool                          m_framebufferReady = false;

    // Auto-reset event the CPU thread signals after publishing a new frame so
    // the idle UI loop blocks on MsgWaitForMultipleObjects instead of spin-
    // polling with Sleep(1). Created/destroyed by RunMessageLoop.
    HANDLE                        m_frameReadyEvent = nullptr;

    // Render-skip gate: the signatures of the last rendered frame's inputs
    // (video mode/soft-switches, flash phase, color mode + text color). Each
    // CPU-thread frame compares the live inputs plus the bus video-dirty flag
    // against these and skips the whole rasterize + publish when nothing that
    // affects the picture has changed. CPU-thread-only (paused during a step).
    uint32_t                      m_lastRenderModeSig  = 0;
    bool                          m_lastRenderFlashOn  = false;
    uint64_t                      m_lastRenderColorSig = 0;

    // Which video mode composed the previous frame. AppleTextMode's dirty-row
    // cache may only reuse a row when the framebuffer still holds that row's
    // text -- so a change of active mode (the buffer last held graphics or
    // another mode) forces a full text re-raster on the next frame.
    class VideoOutput *           m_prevActiveVideoMode = nullptr;

    // The CPU thread's two readings of real time: how long a held key has
    // waited, and whether a Maximum-speed run may publish another frame. The
    // clock behind them is the real one here and a hand-driven one in a test.
    // CPU-thread-only.
    FrameClock                    m_frameClock;

    // Previous UI frame's "any drive live" state, so the loop can force one
    // final present on the live->idle edge and clear the activity LED.
    // The drives' visible state as of the last UI frame, and whether it moved
    // between the two before that -- the present vote asks whether the lamps
    // and doors CHANGED, not whether a motor happens to be energized. See
    // TryPresentUiFrame.
    uint32_t                      m_lastDriveSig     = 0;
    bool                          m_driveSigSettling = false;
};
