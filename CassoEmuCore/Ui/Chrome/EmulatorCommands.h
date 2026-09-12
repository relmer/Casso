#pragma once

#include "Pch.h"

#include "Core/DxuiCommand.h"
#include "Widgets/DxuiMenuBar.h"
#include "Widgets/DxuiPopupMenu.h"
#include "Widgets/DxuiToolbar.h"
#include "Controllers/InputModeRules.h"



class InputClusterEntry;
class PrinterStatusLed;
class VolumeFlyout;





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


// One row of the menu placement table: which title a command sits under,
// and how it reads there. `commandId == 0` is a separator.
struct EmulatorMenuEntry
{
    WORD            commandId;
    MainMenuId      menu;
    const wchar_t * label;
    const wchar_t * accelerator;
    bool            checkable = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands
//
//  Every emulator action declared once. One DxuiCommand per IDM_* the menu
//  bar or the toolbar shows, plus the four toolbar entries that are not
//  commands of the menu's (the theme and monitor-color pickers, the volume
//  flyout, the input cluster), with dispatch, checked, enabled and label
//  bound to the shell through sinks set at startup. Beside the commands sit
//  two placement tables: menu title to item list, and the toolbar's entry
//  list with kinds and groups.
//
//  A command shown on both surfaces has one declaration: the menu reads its
//  mnemonic label and accelerator, the toolbar its short label, glyph and
//  tip. What changes with state -- the fullscreen verb, the mute verb, the
//  machine a reset acts on, the printer card's presence -- is written onto
//  the command by the setters here, so both surfaces read the same word.
//
////////////////////////////////////////////////////////////////////////////////

class EmulatorCommands
{
public:
    using DispatchFn = std::function<void (WORD commandId)>;
    using CheckFn    = std::function<bool (WORD commandId)>;
    using LabelFn    = std::function<std::wstring (WORD commandId)>;

    // Raised when the user picks a row of the paddle-source list.
    using PaddleSourcePickedFn = std::function<void (const InputModeRules::PaddleSource &)>;

    // Ids for the toolbar entries that are not menu commands. Menu command
    // ids start at 40001, so nothing collides.
    static constexpr int  kIdTheme  = 1;
    static constexpr int  kIdColor  = 2;
    static constexpr int  kIdVolume = 3;
    static constexpr int  kIdInput  = 4;

    static constexpr int  kMenuCount = 7;

    EmulatorCommands ();

    // The menu placement table and its helpers, for the parity test and the
    // generated parity document.
    static std::span<const EmulatorMenuEntry>  GetMenuEntries     ();
    static const wchar_t                    *  GetMenuName        (MainMenuId menu);
    static bool                                IsSeparator        (const EmulatorMenuEntry & entry);
    static std::string                         EmitParityMarkdown ();

    // Shell sinks. Read live by every command's functors, so they can be
    // set or replaced at any time without rebuilding anything.
    void  SetDispatch     (DispatchFn fn)   { m_dispatch   = std::move (fn); }
    void  SetCheckQuery   (CheckFn fn)      { m_isChecked  = std::move (fn); }
    void  SetEnableQuery  (CheckFn fn)      { m_isEnabled  = std::move (fn); }

    // Dynamic label override. Consulted live at paint / mnemonic time; an
    // empty return falls back to the entry's static label, so a query only
    // has to answer for the commands it customizes.
    void  SetLabelQuery   (LabelFn fn)      { m_labelQuery = std::move (fn); }

    void  Dispatch        (WORD commandId) const;

    // State the toolbar's words follow.
    void  SetMachineDisplayName (const std::wstring & displayName);
    void  SetFullscreen         (bool fullscreen);
    void  SetMuted              (bool muted);

    // Theme picker: display names in the order the shell holds their ids,
    // and the row the active theme sits on. Monitor color is the fixed
    // Color / Green / Amber / White set the Settings picture list carries.
    void  SetThemeNames         (const std::vector<std::wstring> & displayNames);
    void  SetThemeIndex         (int index)   { m_themeIndex = index; }
    int   GetThemeIndex         () const      { return m_themeIndex; }
    void  SetMonitorColorIndex  (int index)   { m_colorIndex = index; }
    int   GetMonitorColorIndex  () const      { return m_colorIndex; }

    const DxuiCommand *  Find (int commandId) const;

    // The placements. Item lists hold the commands by pointer, so this
    // object must outlive the surfaces it fills.
    std::vector<DxuiMenuBarItem>    BuildMenuItems  () const;
    std::vector<DxuiPopupMenuItem>  GetThemeItems   () const;
    std::vector<DxuiPopupMenuItem>  GetMonitorItems () const;

    // What drives the paddle axes: every attached controller, then the keys
    // and the mouse, exactly one checked (FR-008). The rows are rebuilt only
    // by the setter, so a surface holding the previous list must be handed
    // GetPaddleSourceItems() again.
    void  SetPaddleSources        (const std::vector<InputModeRules::PaddleSource> & sources);
    void  SetPaddleSourcePickedFn (PaddleSourcePickedFn fn) { m_onPaddleSourcePicked = std::move (fn); }

    std::vector<DxuiPopupMenuItem>  GetPaddleSourceItems () const;

    // Fills the toolbar: ten entries in strip order, the LED as the printer
    // entry's decoration, the cluster as the input entry's custom entry and
    // the flyout as the volume entry's panel, with the two pickers' rows and
    // the cluster's rows installed as drop-down lists.
    void  BuildToolbar (DxuiToolbar       & toolbar,
                        PrinterStatusLed  & led,
                        InputClusterEntry & cluster,
                        VolumeFlyout      & volume);

private:
    DxuiCommand *  FindMutable (int commandId);
    void           RebuildActionTips ();


    DispatchFn  m_dispatch;
    CheckFn     m_isChecked;
    CheckFn     m_isEnabled;
    LabelFn     m_labelQuery;

    // Held by pointer from every surface's item list, so the addresses
    // must not move: built once, in the constructor.
    std::vector<std::unique_ptr<DxuiCommand>>  m_commands;
    std::vector<std::unique_ptr<DxuiCommand>>  m_themeRows;
    std::vector<std::unique_ptr<DxuiCommand>>  m_colorRows;
    std::vector<std::unique_ptr<DxuiCommand>>  m_paddleSourceRows;
    std::vector<InputModeRules::PaddleSource>  m_paddleSources;
    PaddleSourcePickedFn                       m_onPaddleSourcePicked;

    std::wstring  m_machineName;
    int           m_themeIndex = -1;
    int           m_colorIndex = 0;
};
