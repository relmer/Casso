#pragma once

#include "Pch.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands
//
//  What the debugger's command bar shows, as DxuiCommands the strip reads at
//  paint and click time.
//
//  THE IDS ARE THE KEY SCHEMES' OWN. Run, Pause, the three steps and Find carry
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
    static constexpr int  kFind        = (int) DebuggerKeySchemes::Action::Find;

    static constexpr int  kShowNext    = 100;
    static constexpr int  kTrace       = 101;

    struct Handlers
    {
        std::function<void (int id)>  dispatch;
        std::function<bool (int id)>  isEnabled;
        std::function<bool (int id)>  isChecked;

        //  Whether the window draws on a dark ground, which picks the icons' colors.
        std::function<bool ()>        isDark;
    };

    //  The colors Visual Studio draws its run and step icons in.
    struct IconColors
    {
        uint32_t  run  = 0;   // Run's triangle
        uint32_t  step = 0;   // the step arrows and their dots
    };

    static IconColors  GetIconColors (bool isDark);

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
        bool                 iconOnly   = false;
    };

    static const std::vector<Row> &  GetRows ();
    static std::wstring              GetTip  (const Row & row, const std::wstring & accelerator);
    static void  PaintIcon        (int id, IDxuiPainter & painter, const DxuiToolbarIconBox & icon, const IconColors & colors);
    static void  PaintRun         (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color);
    static void  PaintPause       (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color);
    static void  PaintStepInto    (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color);
    static void  PaintStepOver    (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color);
    static void  PaintStepOut     (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color);
    static void  PaintRunToCursor (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t arrow, uint32_t bar);
    static void  PaintShowNext    (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color);
    static void  PaintArrowHead   (IDxuiPainter & painter, float tipX, float tipY, float dirX, float dirY, float length, float stroke, uint32_t color);
    static void  PaintStepDot     (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color);

    Handlers                                   m_handlers;
    std::vector<std::shared_ptr<DxuiCommand>>  m_commands;
};
