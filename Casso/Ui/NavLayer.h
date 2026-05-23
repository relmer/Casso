#pragma once

#include "Pch.h"






////////////////////////////////////////////////////////////////////////////////
//
//  NavLayer
//
//  Owns the command parity table mapping every legacy IDM_* command to
//  its parent menu, its label, and its accelerator string. The table is
//  the source of truth for:
//
//      * the markdown traceability doc
//        (specs/007-ui-overhaul/menu-command-parity.md, generated
//        from EmitParityMarkdown);
//      * NavLayerTraceabilityTests, which assert that every published
//        menu IDM_ has a corresponding entry here;
//      * the runtime drop-down list rendered by the native painter
//        (introduced in a later phase).
//
//  Click routing dispatches via a caller-supplied dispatch callback
//  (typically `[this](WORD id) { EmulatorShell::HandleCommand(id); }`)
//  so we never rebuild the menu logic itself — we just route through
//  the existing command pump.
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

    void    Show ();
    void    Hide ();

    void    SetDispatch (DispatchFn dispatch) { m_dispatch = std::move (dispatch); }

    static std::span<const NavCommandEntry> GetCommandEntries ();
    static const wchar_t                  * GetMenuName       (NavMenu menu);
    static std::string                      EmitParityMarkdown ();

    void Dispatch (WORD commandId) const;

private:
    DispatchFn  m_dispatch;
};
