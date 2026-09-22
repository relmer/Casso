#pragma once

#include "Pch.h"
#include "Core/DxuiCommand.h"
#include "Widgets/DxuiToolbar.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands
//
//  What the debugger's command bar shows, as DxuiCommands the strip reads at
//  paint and click time.
//
//  THE IDS ARE THE KEY SCHEMES' OWN. Run, Pause and the three steps carry
//  DebuggerKeySchemes::Action values, so a key and the entry beside it reach
//  the same command by the same id and can never drift apart; the entries a
//  key scheme has no action for take ids above them.
//
//  The window owns the behavior: it hands over one dispatch, one enabled
//  test and one checked test, each taking an id.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerCommands
{
public:
    //  The key schemes' actions, then the bar's own.
    static constexpr int  kRun         = (int) DebuggerKeySchemes::Action::Run;
    static constexpr int  kPause       = (int) DebuggerKeySchemes::Action::Pause;
    static constexpr int  kStepInto    = (int) DebuggerKeySchemes::Action::StepInto;
    static constexpr int  kStepOver    = (int) DebuggerKeySchemes::Action::StepOver;
    static constexpr int  kStepOut     = (int) DebuggerKeySchemes::Action::StepOut;
    static constexpr int  kRunToCursor = (int) DebuggerKeySchemes::Action::RunToCursor;

    static constexpr int  kShowNext    = 100;
    static constexpr int  kTrace       = 101;
    static constexpr int  kPanels      = 102;
    static constexpr int  kKeyScheme   = 103;
    static constexpr int  kMode        = 104;

    struct Handlers
    {
        std::function<void (int id)>  dispatch;
        std::function<bool (int id)>  isEnabled;
        std::function<bool (int id)>  isChecked;
    };

    explicit DebuggerCommands (Handlers handlers);

    std::vector<DxuiToolbar::Entry>  BuildEntries () const;

    //  A command by id, for a label that changes with the machine's state.
    std::shared_ptr<DxuiCommand>  Find (int id) const;

    //  The accelerators the strip shows come from the scheme in force.
    void  ApplyKeyScheme (DebuggerKeyScheme scheme);

private:
    struct Row
    {
        int                  id         = 0;
        const wchar_t      * label      = nullptr;
        const wchar_t      * glyph      = nullptr;
        const wchar_t      * tip        = nullptr;
        DxuiToolbar::Kind    kind       = DxuiToolbar::Kind::Command;
        int                  group      = 0;
        bool                 checkable  = false;
    };

    static const std::vector<Row> &  GetRows ();
    static std::wstring              GetTip  (const Row & row, const std::wstring & accelerator);
    static void  PaintRunToCursor (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t ink);

    Handlers                                   m_handlers;
    std::vector<std::shared_ptr<DxuiCommand>>  m_commands;
};
