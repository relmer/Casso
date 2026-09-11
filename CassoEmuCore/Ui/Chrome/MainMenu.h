#pragma once

#include "Pch.h"

#include "CassoTheme.h"





enum class MainMenuId
{
    File    = 0,
    Edit    = 1,
    Machine = 2,
    Disk    = 3,
    View    = 4,
    Debug   = 5,
    Help    = 6,
};


struct MainMenuCommandEntry
{
    WORD            commandId;
    MainMenuId      menu;
    const wchar_t * label;
    const wchar_t * accelerator;
    bool            checkable = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu
//
//  Casso's application menu bar. Turns the Casso command table into one
//  `DxuiCommand` per entry, with the per-command dispatch, check, enable
//  and label queries (WORD command id -> functor) behind each command's
//  functors, and hands the menu bar one item list per title.
//
//  Visual parity with the legacy chrome is preserved by mirroring the
//  Casso-specific palette (`CassoTheme::nav*Argb` etc.) onto the
//  menu bar via `DxuiMenuBar::SetStripColors` / `SetDropdownColors`
//  before each strip / dropdown paint.
//
////////////////////////////////////////////////////////////////////////////////

class MainMenu : public DxuiMenuBar
{
public:
    using DispatchFn = std::function<void (WORD commandId)>;
    using CheckFn    = std::function<bool (WORD commandId)>;

    MainMenu  ();
    ~MainMenu () override;

    using LabelFn = std::function<std::wstring (WORD commandId)>;

    void                                   SetDispatch        (DispatchFn dispatch);
    void                                   SetCheckQuery      (CheckFn query);
    void                                   SetEnableQuery     (CheckFn query);

    // Dynamic label override. Consulted live at paint / mnemonic time; an
    // empty return falls back to the entry's static label, so a query only
    // has to answer for the commands it customizes.
    void                                   SetLabelQuery      (LabelFn query);

    static std::span<const MainMenuCommandEntry>  GetCommandEntries  ();
    static const wchar_t                       *  GetMenuName        (MainMenuId menu);
    static std::string                            EmitParityMarkdown ();
    static bool                                   IsSeparator        (const MainMenuCommandEntry & entry);

    void  Show           ();
    void  Dispatch       (WORD commandId) const;
    void  PaintStrip     (DxuiPainter             & painter,
                          DxuiTextRenderer        & text,
                          const ChromeVisualState & visual,
                          const CassoTheme       & theme);
    void  PaintDropdown  (DxuiPainter             & painter,
                          DxuiTextRenderer        & text,
                          const ChromeVisualState & visual,
                          const CassoTheme       & theme);
    void  Open           (MainMenuId menu, bool openedByKeyboard);
    using DxuiMenuBar::Open;     // expose int-indexed Open from base

    //
    //  Push the Casso nav/dropdown palette onto the menu bar. Must be
    //  driven on a live path (theme apply / switch) so both the in-window
    //  strip and the popup-backed dropdown use the chrome colors -- the
    //  generic IDxuiTheme mapping is close but not identical.
    //
    void  ApplyChromeColors (const CassoTheme & theme);

    MainMenuId  GetOpenMenu      () const { return (MainMenuId) OpenMenuIndex(); }
    MainMenuId  GetFocusedMenuId () const { return (MainMenuId) DxuiMenuBar::GetFocusedMenu(); }
    using DxuiMenuBar::GetFocusedMenu;

    void        SetFocusedMenu (MainMenuId menu) { DxuiMenuBar::SetFocusedMenu ((int) menu); }
    using DxuiMenuBar::SetFocusedMenu;

    static constexpr int  kMenuCount = 7;

private:
    void  Rebuild           ();


    DispatchFn  m_dispatch;
    CheckFn     m_isChecked;
    CheckFn     m_isEnabled;
    LabelFn     m_labelQuery;

    //  One command per table entry that is not a separator. Held by pointer
    //  from the menu bar's item lists, so the addresses must not move while
    //  the bar exists; a fresh vector is built on every Rebuild and the bar
    //  is closed first.
    std::vector<std::unique_ptr<DxuiCommand>>  m_commands;
};
