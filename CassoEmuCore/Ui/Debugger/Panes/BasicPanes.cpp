#include "Pch.h"

#include "Ui/Debugger/Panes/BasicPanes.h"

#include "Core/TextEncoding.h"





static constexpr const wchar_t * s_kpszNoApplesoft = L"Applesoft is not in memory.";
static constexpr const wchar_t * s_kpszNoProgram   = L"No Applesoft program is in memory.";





////////////////////////////////////////////////////////////////////////////////
//
//  MakeNote
//
//  One muted cell across the row.
//
////////////////////////////////////////////////////////////////////////////////

static std::vector<DxuiListView::Cell> MakeNote (const std::wstring & text)
{
    std::vector<DxuiListView::Cell>  cells (1, DxuiListView::Cell { text, true });



    cells[0].spansRow = true;
    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramPane::BasicProgramPane
//
////////////////////////////////////////////////////////////////////////////////

BasicProgramPane::BasicProgramPane (DxuiListView * list, ToggleFn toggle) :
    m_list   (list),
    m_toggle (std::move (toggle))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramPane::Configure
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgramPane::Configure (int gutterDip)
{
    m_list->SetColumns ({ { L"",        gutterDip, false, DxuiTextHAlign::Left },
                          { L"Listing", 0,         true,  DxuiTextHAlign::Left } });

    m_list->SetActivateOnDoubleClick (true);

    m_list->SetOnActivateRow ([this] (int row)
    {
        if (row >= 0 && row < (int) m_rowLines.size())
        {
            m_toggle (m_rowLines[(size_t) row]);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramPane::Apply
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgramPane::Apply (
    const std::optional<DebuggerViewSnapshot::BasicState>    & basic,
    const std::vector<DebuggerViewSnapshot::BreakpointLine>  & breakpoints)
{
    std::vector<std::vector<DxuiListView::Cell>>  cells;
    std::wstring                                  note = GetNote (basic);



    m_rows.clear();
    m_rowLines.clear();

    if (!note.empty() || !basic.has_value())
    {
        cells.push_back (MakeNote (note));
        m_list->SetRows (std::move (cells));
        return;
    }

    m_rows = GetRows (basic->lines);
    cells  = GetCells (*basic, m_rows, breakpoints, m_style);

    for (const Row & row : m_rows)
    {
        m_rowLines.push_back (basic->lines[row.lineIndex].number);
    }

    m_list->SetRows (std::move (cells));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramPane::ShowLine
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgramPane::ShowLine (Word line)
{
    auto  found = std::ranges::find (m_rowLines, line);



    if (found != m_rowLines.end())
    {
        m_list->SetSelectedRow ((int) (found - m_rowLines.begin()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramPane::GetNote
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BasicProgramPane::GetNote (const std::optional<DebuggerViewSnapshot::BasicState> & basic)
{
    if (!basic.has_value())
    {
        return std::wstring();
    }

    if (!basic->isApplesoft)
    {
        return s_kpszNoApplesoft;
    }

    return (!basic->hasProgram || basic->lines.empty()) ? std::wstring (s_kpszNoProgram) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramPane::GetRows
//
//  LIST's layout, as ApplesoftProgram::GetListRows has it: a space, the
//  number and a space, then the pieces, a new row five columns in once a
//  row is 33 columns long, tested before each piece.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<BasicProgramPane::Row> BasicProgramPane::GetRows (const std::vector<BasicLine> & lines)
{
    std::vector<Row>  rows;



    for (size_t index = 0; index < lines.size(); index++)
    {
        const BasicLine  & line = lines[index];
        Row                row;



        row.lineIndex = index;
        row.text      = std::format (L" {} ", line.number);

        for (size_t piece = 0; piece < line.pieces.size(); piece++)
        {
            if ((int) row.text.size() >= ApplesoftProgram::kListWrap)
            {
                rows.push_back (row);
                row         = Row();
                row.lineIndex = index;
                row.isFirst = false;
                row.text    = std::wstring (ApplesoftProgram::kListIndent, L' ');
            }

            row.pieces.push_back ({ (int) piece, (int) row.text.size() });
            row.text += TextEncoding::NarrowToWide (line.pieces[piece]);
        }

        rows.push_back (row);
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramPane::GetStatement
//
//  A colon ends a statement unless it is inside quotes or after a REM; the
//  colon is part of the statement it ends.
//
////////////////////////////////////////////////////////////////////////////////

std::pair<int, int> BasicProgramPane::GetStatement (const BasicLine & line, int piece)
{
    int   first     = 0;
    int   count     = (int) line.pieces.size();
    bool  isQuoted  = false;
    bool  isRemark  = false;



    for (int at = 0; at < count; at++)
    {
        const std::string  & text  = line.pieces[(size_t) at];
        bool                 isEnd = text == ":" && !isQuoted && !isRemark;



        isQuoted = (text == "\"") ? !isQuoted : isQuoted;
        isRemark = isRemark || (!isQuoted && text == " REM ");

        if (isEnd && at >= piece)
        {
            return { first, at + 1 };
        }

        first = isEnd ? at + 1 : first;
    }

    return { first, count };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramPane::GetCells
//
//  The line being run has the PC's row color on every row; its statement is
//  drawn as the rest of the program is and the line's other statements are
//  muted, and the PC's arrow is on the row the statement starts on. With
//  TXTPTR past the line's last piece, as after its last statement, the
//  whole line is in full color.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<DxuiListView::Cell>> BasicProgramPane::GetCells (
    const DebuggerViewSnapshot::BasicState                  & basic,
    const std::vector<Row>                                  & rows,
    const std::vector<DebuggerViewSnapshot::BreakpointLine> & breakpoints,
    const Style                                             & style)
{
    std::vector<std::vector<DxuiListView::Cell>>  cells;



    for (const Row & row : rows)
    {
        const BasicLine                  & line       = basic.lines[row.lineIndex];
        std::vector<DxuiListView::Cell>    rowCells (2);
        bool                               isCurrent  = basic.line == line.number;
        int                                piece      = (int) basic.textPointer - (int) (line.address + 4);
        bool                               isInLine   = isCurrent && piece >= 0 && piece < (int) line.pieces.size();
        std::pair<int, int>                statement  = isInLine ? GetStatement (line, piece) : std::pair<int, int> (0, (int) line.pieces.size());
        auto                               breakpoint = std::ranges::find_if (breakpoints, [&] (const DebuggerViewSnapshot::BreakpointLine & bp)
        {
            return bp.info.kind == BreakpointKind::BasicLine && bp.info.address == line.number;
        });
        bool                               hasDot     = row.isFirst && breakpoint != breakpoints.end();
        bool                               isEnabled  = hasDot && breakpoint->enabled;
        bool                               hasArrow   = isCurrent && std::ranges::any_of (row.pieces, [&] (const std::pair<int, int> & p) { return p.first == statement.first; });



        hasArrow = hasArrow || (isCurrent && row.isFirst && line.pieces.empty());

        rowCells[1].text = row.text;
        rowCells[0].icon = hasArrow ? (!hasDot ? style.pcIcon : isEnabled ? style.pcOnEnabledIcon : style.pcOnDisabledIcon)
                         : hasDot   ? (isEnabled ? style.enabledIcon : style.disabledIcon)
                         :            nullptr;

        AddSyntaxColors (line, row, style.syntax, rowCells[1]);

        if (isCurrent)
        {
            for (DxuiListView::Cell & cell : rowCells)
            {
                cell.background = style.pcRowArgb;
            }

            for (size_t at = 0; at < row.pieces.size() && isInLine; at++)
            {
                int  index = row.pieces[at].first;
                int  start = row.pieces[at].second;
                int  end   = (at + 1 < row.pieces.size()) ? row.pieces[at + 1].second : (int) row.text.size();



                if (index < statement.first || index >= statement.second)
                {
                    rowCells[1].dimRanges.push_back ({ start, end });
                }
            }
        }

        cells.push_back (std::move (rowCells));
    }

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramPane::AddSyntaxColors
//
//  The line number in the address color, keywords in the mnemonic color, a
//  string from quote to quote in the string color, and a remark after REM in
//  the comment color.
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgramPane::AddSyntaxColors (const BasicLine & line, const Row & row, const SourceSyntax::Colors & syntax, DxuiListView::Cell & cell)
{
    bool  isQuoted = false;
    bool  isRemark = false;



    if (syntax.mnemonic == 0)
    {
        return;
    }

    if (row.isFirst)
    {
        cell.colorRanges.push_back ({ 1, 1 + (int) std::to_wstring (line.number).size(), syntax.address });
    }

    for (int piece = 0; piece < (int) line.pieces.size(); piece++)
    {
        const std::string  & text      = line.pieces[(size_t) piece];
        bool                 isKeyword = text.size() > 1 && text.front() == ' ';
        bool                 isQuote   = text == "\"";
        auto                 found     = std::ranges::find_if (row.pieces, [piece] (const std::pair<int, int> & p) { return p.first == piece; });
        uint32_t             argb      = 0;



        argb = isRemark                          ? syntax.comment
             : (isQuoted || isQuote)             ? syntax.string
             : isKeyword                         ? syntax.mnemonic
             :                                     0;

        if (found != row.pieces.end() && argb != 0)
        {
            int  start = found->second;
            int  end   = start + (int) text.size();



            //  A run of one color is one range, as ranges may not overlap.
            if (!cell.colorRanges.empty() && std::get<1> (cell.colorRanges.back()) == start && std::get<2> (cell.colorRanges.back()) == argb)
            {
                std::get<1> (cell.colorRanges.back()) = end;
            }
            else
            {
                cell.colorRanges.push_back ({ start, end, argb });
            }
        }

        isQuoted = isQuote ? !isQuoted : isQuoted;
        isRemark = isRemark || (!isQuoted && text == " REM ");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicWatches::IsStoppedInProgram
//
////////////////////////////////////////////////////////////////////////////////

bool BasicWatches::IsStoppedInProgram (const std::optional<DebuggerViewSnapshot::BasicState> & basic)
{
    return basic.has_value() && basic->isApplesoft && basic->hasValues && basic->line.has_value();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicWatches::TryGetValue
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::string> BasicWatches::TryGetValue (const DebuggerViewSnapshot::BasicState & basic, const std::string & text)
{
    BasicReference  reference;
    size_t          index     = 0;
    size_t          stride    = 1;



    if (!ApplesoftProgram::TryParseReference (text, reference))
    {
        return std::nullopt;
    }

    if (reference.subscripts.empty())
    {
        for (const BasicVariable & variable : basic.variables)
        {
            if (variable.name == reference.name)
            {
                return Quote (variable.type, variable.value);
            }
        }

        return Quote (reference.type, reference.type == BasicType::String ? std::string() : std::string ("0"));
    }

    for (const BasicArray & array : basic.arrays)
    {
        if (array.name != reference.name || array.sizes.size() != reference.subscripts.size())
        {
            continue;
        }

        for (size_t at = 0; at < array.sizes.size(); at++)
        {
            if (reference.subscripts[at] >= array.sizes[at])
            {
                return std::nullopt;
            }

            index  += (size_t) reference.subscripts[at] * stride;
            stride *= (size_t) std::max (array.sizes[at], 1);
        }

        return (index < array.values.size()) ? std::optional<std::string> (Quote (array.type, array.values[index])) : std::nullopt;
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicWatches::GetStatementItems
//
//  A variable is a run of single letters and digits starting with a letter,
//  with a $ or % after it; a keyword is one piece of its own. Text in quotes,
//  a REM and a DATA's items are not variables, and an array is left out,
//  since its subscripts are expressions.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<BasicWatches::Item> BasicWatches::GetStatementItems (const DebuggerViewSnapshot::BasicState & basic)
{
    std::vector<Item>         items;
    std::vector<std::string>  seen;
    const BasicLine         * line      = nullptr;
    int                       piece     = 0;
    std::pair<int, int>       statement;
    bool                      isQuoted  = false;
    bool                      isSkipped = false;
    std::string               name;
    auto                      isNamePiece = [] (const std::string & text, bool isFirst)
    {
        return text.size() == 1 && (std::isupper ((unsigned char) text[0]) || (!isFirst && std::isdigit ((unsigned char) text[0])));
    };



    if (!basic.line.has_value())
    {
        return items;
    }

    for (const BasicLine & each : basic.lines)
    {
        line = (each.number == *basic.line) ? &each : line;
    }

    if (line == nullptr)
    {
        return items;
    }

    piece     = std::clamp ((int) basic.textPointer - (int) (line->address + 4), 0, std::max ((int) line->pieces.size() - 1, 0));
    statement = BasicProgramPane::GetStatement (*line, piece);

    for (int at = statement.first; at < statement.second; at++)
    {
        const std::string  & text = line->pieces[(size_t) at];



        isQuoted  = (text == "\"") ? !isQuoted : isQuoted;
        isSkipped = isSkipped || (!isQuoted && (text == " REM " || text == " DATA "));

        if (isQuoted || isSkipped || !isNamePiece (text, true))
        {
            continue;
        }

        name = text;

        while (at + 1 < statement.second && isNamePiece (line->pieces[(size_t) at + 1], false))
        {
            name += line->pieces[(size_t) ++at];
        }

        if (at + 1 < statement.second && (line->pieces[(size_t) at + 1] == "$" || line->pieces[(size_t) at + 1] == "%"))
        {
            name += line->pieces[(size_t) ++at];
        }

        if (at + 1 < statement.second && line->pieces[(size_t) at + 1] == "(")
        {
            continue;
        }

        if (std::ranges::find (seen, name) == seen.end())
        {
            seen.push_back (name);
            items.push_back ({ name, TryGetValue (basic, name).value_or ("") });
        }
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicWatches::GetAllItems
//
////////////////////////////////////////////////////////////////////////////////

std::vector<BasicWatches::Item> BasicWatches::GetAllItems (const DebuggerViewSnapshot::BasicState & basic)
{
    std::vector<Item>  items;



    for (const BasicVariable & variable : basic.variables)
    {
        items.push_back ({ variable.name, Quote (variable.type, variable.value) });
    }

    for (const BasicArray & array : basic.arrays)
    {
        for (size_t index = 0; index < array.values.size(); index++)
        {
            items.push_back ({ GetElementName (array, index), Quote (array.type, array.values[index]) });
        }
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicWatches::GetElementName
//
////////////////////////////////////////////////////////////////////////////////

std::string BasicWatches::GetElementName (const BasicArray & array, size_t index)
{
    std::string  subscripts;
    size_t       rest       = index;



    for (int size : array.sizes)
    {
        size_t  count = (size_t) std::max (size, 1);



        subscripts += std::format ("{}{}", subscripts.empty() ? "" : ",", rest % count);
        rest       /= count;
    }

    return array.name + "(" + subscripts + ")";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicWatches::MakeNameCell
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::Cell BasicWatches::MakeNameCell (const std::string & name)
{
    static constexpr int  kReadLength = 2;
    DxuiListView::Cell    cell        = { TextEncoding::NarrowToWide (name) };
    size_t                letters     = name.find_first_of ("$%(");



    letters = (letters == std::string::npos) ? name.size() : letters;

    if ((int) letters > kReadLength)
    {
        cell.dimRanges.push_back ({ kReadLength, (int) letters });
    }

    return cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicWatches::Quote
//
////////////////////////////////////////////////////////////////////////////////

std::string BasicWatches::Quote (BasicType type, const std::string & value)
{
    return (type == BasicType::String) ? "\"" + value + "\"" : value;
}
