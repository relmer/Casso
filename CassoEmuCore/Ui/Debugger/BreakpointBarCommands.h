#pragma once

#include "Pch.h"
#include "Core/DxuiCommand.h"
#include "Widgets/DxuiToolbar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointBarCommands
//
//  The breakpoints pane's toolbar, after Visual Studio's Breakpoints window
//  (FR-119): New, Delete, Delete all, Enable all, Disable all, Undo, Redo, Go
//  to source code, Go to disassembly, Show columns, Export and Import, each an
//  icon with a tip. The bar is a DxuiToolbar, so what does not fit goes into
//  its "..." menu as on every other strip.
//
//  The window owns the behavior: it hands over one dispatch and one enabled
//  test, each taking an id.
//
////////////////////////////////////////////////////////////////////////////////

class BreakpointBarCommands
{
public:
    static constexpr int  kNew            = 1;
    static constexpr int  kDelete         = 2;
    static constexpr int  kDeleteAll      = 3;
    static constexpr int  kEnableAll      = 4;
    static constexpr int  kDisableAll     = 5;
    static constexpr int  kUndo           = 6;
    static constexpr int  kRedo           = 7;
    static constexpr int  kGoToSource     = 8;
    static constexpr int  kGoToCode       = 9;
    static constexpr int  kColumns        = 10;
    static constexpr int  kExport         = 11;
    static constexpr int  kImport         = 12;

    struct Handlers
    {
        std::function<void (int id)>  dispatch;
        std::function<bool (int id)>  isEnabled;
    };

    explicit BreakpointBarCommands (Handlers handlers);

    std::vector<DxuiToolbar::Entry>  BuildEntries () const;
    std::shared_ptr<DxuiCommand>     Find         (int id) const;

    //  Every id, in the order the bar shows them.
    static std::vector<int>          GetIds       ();

private:
    struct Row
    {
        int                  id    = 0;
        const wchar_t      * label = nullptr;
        const wchar_t      * glyph = nullptr;
        const wchar_t      * tip   = nullptr;
        DxuiToolbar::Kind    kind  = DxuiToolbar::Kind::Command;
        int                  group = 0;
    };

    static const std::vector<Row> &  GetRows ();

    Handlers                                   m_handlers;
    std::vector<std::shared_ptr<DxuiCommand>>  m_commands;
};
