#include "Pch.h"

#include "Ui/Debugger/Panes/DisassemblySymbols.h"

#include "Ui/Debugger/Panes/DisassemblyViews.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Debugger/SymbolDescriptions.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/HeatStrip.h"
#include "Ui/Debugger/HistoryBand.h"
#include "Ui/Debugger/SourceLineText.h"
#include "Ui/Debugger/Panes/CodeEditCommands.h"
#include "Ui/Debugger/Panes/CodeViewFind.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/DisassemblyGutter.h"
#include "Ui/Debugger/Panes/SourceDocumentPanes.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/SourceRows.h"
#include "Ui/Debugger/ToolbarCheckEntry.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblySymbols::TryGetSymbolTip
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblySymbols::TryGetSymbolTip (POINT clientPx, RECT & anchor, std::wstring & text) const
{
    DxuiListView * list    = nullptr;
    RECT           bounds  = {};
    int            view    = -1;
    int            row     = -1;
    int            column  = -1;
    int            index   = -1;
    std::string    name;
    Word           address = 0;



    list = m_group.FindCodeListAt (clientPx, view);

    if (m_host.GetLatestSnapshot() == nullptr || list == nullptr)
    {
        return false;
    }

    bounds = list->GetBounds();
    row    = list->HitTestRow (clientPx.x - bounds.left, clientPx.y - bounds.top);
    column = m_host.GetListColumnAt (list, clientPx.x - bounds.left);
    index  = m_group.GetCodeLineOfRow (view, row);

    if (index < 0)
    {
        return TryGetSourceSymbolTip (*list, clientPx, anchor, text);
    }

    GetLineSymbol (m_group.GetCodeLines (view)[(size_t) index], column, name, address);

    if (!list->GetCellTextRectPx (row, (size_t) column, anchor))
    {
        return false;
    }

    //  An operand with no symbol at a card's or port's register has the
    //  register's description.
    if (name.empty())
    {
        OffsetRect (&anchor, bounds.left, bounds.top);
        return column == (int) DisassemblyCells::kInstructionColumn && TryGetIoRegisterTip (m_host.GetLatestSnapshot()->ioRegisters, address, text);
    }

    text = std::format (L"{} = ${:04X}", TextEncoding::NarrowToWide (name), address);
    SourceLineText::AppendSymbolDetails (text, name, address, false, *m_host.GetLatestSnapshot(), m_host.GetSymbolComment (address, name));

    OffsetRect (&anchor, bounds.left, bounds.top);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblySymbols::TryGetSourceSymbolTip
//
//  Over a word of a source row, the symbol it is.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblySymbols::TryGetSourceSymbolTip (const DxuiListView & list, POINT clientPx, RECT & anchor, std::wstring & text) const
{
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
    RECT                          bounds   = list.GetBounds();
    int                           row      = -1;
    size_t                        column   = 0;
    int                           ch       = 0;
    const SymbolInfo            * found    = nullptr;



    if (snapshot == nullptr || snapshot->symbols == nullptr ||
        !list.TryHitTestCellChar (POINT { clientPx.x - bounds.left, clientPx.y - bounds.top }, row, column, ch) ||
        !SourceLineText::TryGetSymbolText (list.GetCellsOfRow (row)[column].text, ch, snapshot->symbols->symbols, text, &found))
    {
        return false;
    }

    SourceLineText::AppendSymbolDetails (text, found->name, found->address, found->isConstant, *snapshot, m_host.GetSymbolComment (found->address, found->name));
    anchor = RECT { clientPx.x, clientPx.y, clientPx.x + 1, clientPx.y + 1 };
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblySymbols::TryGetSymbolAt
//
//  The symbol under a point in a code view: an instruction row's label or
//  operand, or a word of a source row that is a symbol's name.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblySymbols::TryGetSymbolAt (POINT clientPx, std::string & name, Word & address) const
{
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
    DxuiListView                * list     = nullptr;
    const SymbolInfo            * found    = nullptr;
    RECT                          bounds   = {};
    int                           view     = -1;
    int                           row      = -1;
    int                           index    = -1;
    size_t                        column   = 0;
    int                           ch       = 0;
    int                           start    = 0;



    name.clear();
    address = 0;
    list    = m_group.FindCodeListAt (clientPx, view);

    if (snapshot == nullptr || list == nullptr)
    {
        return false;
    }

    bounds = list->GetBounds();
    row    = list->HitTestRow (clientPx.x - bounds.left, clientPx.y - bounds.top);
    index  = m_group.GetCodeLineOfRow (view, row);

    if (index >= 0)
    {
        GetLineSymbol (m_group.GetCodeLines (view)[(size_t) index], m_host.GetListColumnAt (list, clientPx.x - bounds.left), name, address);
        return !name.empty();
    }

    if (snapshot->symbols != nullptr && list->TryHitTestCellChar (POINT { clientPx.x - bounds.left, clientPx.y - bounds.top }, row, column, ch))
    {
        found = SourceLineText::FindSymbolAt (list->GetCellsOfRow (row)[column].text, ch, snapshot->symbols->symbols, start);
    }

    if (found == nullptr)
    {
        return false;
    }

    name    = found->name;
    address = found->address;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblySymbols::TryGetOperandSymbol
//
//  A source row's code is in the cell, and from the character, that its
//  parts indicate: the instruction column's start while the label column
//  is visible, and past the label while the whole line is in one cell.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblySymbols::TryGetOperandSymbol (int view, int row, std::wstring & text) const
{
    const DebuggerViewSnapshot     * snapshot = m_host.GetLatestSnapshot();
    DxuiListView                   * list     = (view >= 0 && view < DebuggerViewState::kMaxCodeViews) ? m_group.GetCodeList (view) : nullptr;
    int                              index    = (list != nullptr) ? m_group.GetCodeLineOfRow (view, row) : -1;
    std::string                      name;
    Word                             address  = 0;
    DisassemblyCells::SourceParts    parts;



    if (snapshot == nullptr || list == nullptr || row < 0 || row >= list->GetRowCount())
    {
        return false;
    }

    if (index >= 0)
    {
        GetLineSymbol (m_group.GetCodeLines (view)[(size_t) index], (int) DisassemblyCells::kInstructionColumn, name, address);

        if (name.empty())
        {
            return false;
        }

        text = std::format (L"{}=${:04X}", TextEncoding::NarrowToWide (name), address);
        return true;
    }

    return snapshot->symbols != nullptr && m_group.TryGetSourceParts (view, row, parts) && list->GetCellsOfRow (row).size() > parts.codeColumn &&
           SourceLineText::TryGetOperandSymbolText (list->GetCellsOfRow (row)[parts.codeColumn].text.substr (0, (size_t) GetCodeEnd (parts)), parts.codeChar,
                                                    snapshot->symbols->symbols, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblySymbols::IsOperandSymbolAt
//
//  An instruction's operand's symbol is the one in its instruction column; a
//  source row's is the first symbol past its mnemonic, in the cell its code
//  is in.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblySymbols::IsOperandSymbolAt (POINT clientPx) const
{
    const DebuggerViewSnapshot     * snapshot = m_host.GetLatestSnapshot();
    int                              view     = -1;
    DxuiListView                   * list     = m_group.FindCodeListAt (clientPx, view);
    RECT                             bounds   = {};
    int                              row      = -1;
    size_t                           column   = 0;
    int                              ch       = 0;
    DisassemblyCells::SourceParts    parts;



    if (snapshot == nullptr || list == nullptr)
    {
        return false;
    }

    bounds = list->GetBounds();
    row    = list->HitTestRow (clientPx.x - bounds.left, clientPx.y - bounds.top);

    if (m_group.GetCodeLineOfRow (view, row) >= 0)
    {
        return m_host.GetListColumnAt (list, clientPx.x - bounds.left) == (int) DisassemblyCells::kInstructionColumn;
    }

    return snapshot->symbols != nullptr && m_group.TryGetSourceParts (view, row, parts) &&
           list->TryHitTestCellChar (POINT { clientPx.x - bounds.left, clientPx.y - bounds.top }, row, column, ch) && column == parts.codeColumn &&
           SourceLineText::IsOperandSymbolAt (list->GetCellsOfRow (row)[column].text.substr (0, (size_t) GetCodeEnd (parts)), parts.codeChar, ch,
                                              snapshot->symbols->symbols);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblySymbols::TryGetIoRegisterTip
//
//  "$C0A9" and the register's description on the line below, for an
//  address with one.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblySymbols::TryGetIoRegisterTip (const std::vector<DiagnosticsIoRegister> & registers, Word address, std::wstring & text)
{
    for (const DiagnosticsIoRegister & each : registers)
    {
        if (each.address == address)
        {
            text = std::format (L"${:04X}\n{}", address, TextEncoding::NarrowToWide (each.description));
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblySymbols::GetLineSymbol
//
//  The symbol in a column of an instruction's row: its label in the Label
//  column, or the symbol its operand is written with in the Instruction
//  column, and its address. The operand text differs from the address only
//  for a symbol.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblySymbols::GetLineSymbol (const DebuggerViewSnapshot::CodeLine & line, int column, std::string & name, Word & address)
{
    if (column == (int) DisassemblyCells::kInstructionColumn - 1 && !line.label.empty())
    {
        name    = line.label;
        address = line.address;
        return;
    }

    if (column != (int) DisassemblyCells::kInstructionColumn)
    {
        return;
    }

    for (char ch : (line.shownOperand != line.memoryOperand) ? line.shownOperand : std::string())
    {
        if (std::isalnum ((unsigned char) ch) || ch == '_')
        {
            name += ch;
        }
        else if (!name.empty())
        {
            break;
        }
    }

    for (char ch : line.memoryOperand)
    {
        if (std::isxdigit ((unsigned char) ch))
        {
            address = (Word) ((address << 4) | (Word) std::stoi (std::string (1, ch), nullptr, 16));
        }
        else if (ch == ',' || ch == ')')
        {
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblySymbols::AddOperandSymbol
//
//  The symbol the instruction's operand is written with, "COUT=$FDED",
//  ahead of the operand column's values, its name in the symbol color, so
//  it is read without the operand tip.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblySymbols::AddOperandSymbol (DxuiListView::Cell & cell, const DebuggerViewSnapshot::CodeLine & line, uint32_t symbolArgb)
{
    std::string   name;
    Word          address = 0;
    std::wstring  symbol;
    int           shift   = 0;



    GetLineSymbol (line, (int) DisassemblyCells::kInstructionColumn, name, address);

    if (name.empty())
    {
        return;
    }

    symbol = std::format (L"{}=${:04X}", TextEncoding::NarrowToWide (name), address);
    shift  = (int) symbol.size() + (cell.text.empty() ? 0 : 2);

    for (auto & [start, end, argb] : cell.colorRanges)
    {
        start += shift;
        end   += shift;
    }

    cell.colorRanges.insert (cell.colorRanges.begin(), { 0, (int) name.size(), symbolArgb });
    cell.text = symbol + (cell.text.empty() ? std::wstring() : L"  " + cell.text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblySymbols::GetCodeEnd
//
//  Where a source row's line ends in the cell its code is in: before the
//  operand and result where they follow the line in that cell.
//
////////////////////////////////////////////////////////////////////////////////

int DisassemblySymbols::GetCodeEnd (const DisassemblyCells::SourceParts & parts)
{
    return (parts.operandColumn == parts.codeColumn) ? parts.operandChar : INT_MAX;
}
