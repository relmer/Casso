#pragma once

#include "Pch.h"






////////////////////////////////////////////////////////////////////////////////
//
//  NavLayer
//
//  P4 D3D-rendered nav layer that mirrors the Win32 menu bar. Holds a
//  command parity table mapping every legacy IDM_* command to its
//  parent menu, its label, and its accelerator string. The same table
//  is the source of truth for:
//
//      * the markdown traceability doc
//        (specs/007-ui-overhaul/menu-command-parity.md, generated
//        from EmitParityMarkdown);
//      * the NavLayerTraceabilityTests which assert that every
//        published menu IDM_ has a corresponding entry here;
//      * the runtime drop-down list rendered into the RML doc.
//
//  Click routing dispatches via a caller-supplied dispatch callback
//  (typically `[this](WORD id) { EmulatorShell::HandleCommand(id); }`)
//  so we never rebuild the menu logic itself — we just route through
//  the existing command pump.
//
//  P4 parallel-mode invariant: the Win32 menu bar STAYS in place; the
//  nav layer renders on top of the same chrome region. P9 retires the
//  Win32 path.
//
////////////////////////////////////////////////////////////////////////////////

enum class NavMenu
{
    File    = 0,
    Edit    = 1,
    Machine = 2,
    Disk    = 3,
    View    = 4,
    Help    = 5,
};


struct NavCommandEntry
{
    WORD            commandId;
    NavMenu         menu;
    const wchar_t * label;
    const wchar_t * accelerator;   // may be nullptr
};


class NavLayer
{
public:
    using DispatchFn = std::function<void (WORD commandId)>;


    NavLayer  ();
    ~NavLayer ();

    HRESULT Show (Rml::Context * context, DispatchFn dispatch);
    void    Hide ();

    static std::span<const NavCommandEntry> GetCommandEntries ();
    static const wchar_t                  * GetMenuName       (NavMenu menu);
    static std::string                      EmitParityMarkdown ();

    void Dispatch (WORD commandId) const;

private:
    Rml::Context         * m_context  = nullptr;
    Rml::ElementDocument * m_doc      = nullptr;
    DispatchFn             m_dispatch;
};
