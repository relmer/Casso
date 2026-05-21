#pragma once

#include "Pch.h"

#include "RmlBackend_D3D11.h"
#include "RmlSystemInterface.h"
#include "RmlFontEngine_DWrite.h"

#include <RmlUi/Core/Context.h>





////////////////////////////////////////////////////////////////////////////////
//
//  UiShell
//
//  The single owner of every RmlUi global state for one HWND. Wires
//  the D3D11 render interface, the Win32 system interface, and the
//  DirectWrite font engine into Rml::Initialise + Rml::CreateContext.
//
//  Lifecycle
//  ---------
//      Initialize (renderer, hwnd)
//          -> binds interfaces to RmlUi, calls Rml::Initialise,
//             creates a context sized to the HWND client rect.
//      OnResize (w, h)
//          -> forwards to the backend and the context.
//      Render()
//          -> intended to be invoked from D3DRenderer's per-frame
//             hook, between the emulator blit and Present. Calls
//             Context::Update + Context::Render which funnels through
//             the bound render interface.
//      Shutdown()
//          -> destroys the context, calls Rml::Shutdown, releases the
//             interfaces in reverse order.
//
//  Parallel-mode invariant (P3-T5): existing Win32 menus, dialogs,
//  and the framebuffer-blit path are NOT touched. The UiShell renders
//  *after* the emulator blit so any RmlUi content composites on top.
//
////////////////////////////////////////////////////////////////////////////////

class D3DRenderer;
class IFileSystem;


class UiShell
{
public:
    UiShell();
    ~UiShell();

    HRESULT Initialize (
        D3DRenderer  & renderer,
        HWND           hwnd,
        IFileSystem  * pFs);

    void    Shutdown();

    HRESULT OnResize (int widthPx, int heightPx);

    // Per-frame entry point. Safe to call after Shutdown — becomes a
    // no-op so D3DRenderer's hook can fire harmlessly during the
    // teardown window.
    void    Render();

    Rml::Context * GetContext() const { return m_context; }

private:
    RmlSystemInterface       m_system;
    RmlFontEngine_DWrite     m_fonts;
    RmlBackend_D3D11         m_backend;

    Rml::Context           * m_context        = nullptr;

    HWND                     m_hwnd           = nullptr;
    int                      m_widthPx        = 0;
    int                      m_heightPx       = 0;

    bool                     m_rmlInitialised = false;

    // Diagnostic — counts Render() invocations so callers can sanity
    // check the per-frame hook is wired.
    UINT                     m_frameCount     = 0;
};
