#pragma once

#include "Ui/Debugger/DebuggerViewState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ICodeViews
//
//  The disassembly views' lines and rows, for the parts of a view that act
//  on them: its edit commands, its gutter and its tips.
//
////////////////////////////////////////////////////////////////////////////////

class ICodeViews
{
public:
    virtual ~ICodeViews() = default;

    //  A view's lines, its list, and the instruction in a row of its list,
    //  as an index into its lines, or -1 for a source row.
    virtual const std::vector<DebuggerViewSnapshot::CodeLine> &  GetCodeLines (int view) const = 0;
    virtual DxuiListView *                                       GetCodeList  (int view) const = 0;
    virtual int                                                  GetCodeLineOfRow (int view, int row) const = 0;
};
