#pragma once

#include "Pch.h"

#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/Panes/DebuggerPaneParts.h"

class DisassemblyGroup;
struct DiagnosticsIoRegister;





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblySymbols
//
//  The symbols in the disassembly views: over a symbol in a view -- a
//  line's label, or the symbol an operand is written with -- its address and
//  its tip, and the symbol a row's operand is written with.
//
////////////////////////////////////////////////////////////////////////////////

class DisassemblySymbols
{
public:
    DisassemblySymbols (const DebuggerPaneParts & parts, const DisassemblyGroup & group) : m_windowHost (parts.windowHost), m_symbolComments (parts.symbolComments), m_latest (parts.latest), m_group (group) {}

    bool  TryGetSymbolTip       (POINT clientPx, RECT & anchor, std::wstring & text) const;
    bool  TryGetSourceSymbolTip (const DxuiListView & list, POINT clientPx, RECT & anchor, std::wstring & text) const;
    bool  TryGetSymbolAt        (POINT clientPx, std::string & name, Word & address) const;

    //  The symbol a row's operand is written with, as "name = $value": an
    //  instruction's, or the first a source row's code has after its
    //  mnemonic. False for a row with none. And whether the symbol under a
    //  point is the one its row's operand is written with.
    bool  TryGetOperandSymbol   (int view, int row, std::wstring & text) const;
    bool  IsOperandSymbolAt     (POINT clientPx) const;

    static bool  TryGetIoRegisterTip (const std::vector<DiagnosticsIoRegister> & registers, Word address, std::wstring & text);
    static void  GetLineSymbol       (const DebuggerViewSnapshot::CodeLine & line, int column, std::string & name, Word & address);
    static void  AddOperandSymbol    (DxuiListView::Cell & cell, const DebuggerViewSnapshot::CodeLine & line, uint32_t symbolArgb);
    static int   GetCodeEnd          (const DisassemblyCells::SourceParts & parts);

private:
    IDebuggerWindowHost * const  & m_windowHost;
    SymbolComments               & m_symbolComments;
    const LatestSnapshot         & m_latest;
    const DisassemblyGroup       & m_group;
};
