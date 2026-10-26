#include "Pch.h"

#include "Ui/Debugger/Panes/CodeEditCommands.h"

#include "Core/TextEncoding.h"
#include "Debugger/LineAssembler.h"
#include "OpcodeTable.h"
#include "Ui/Debugger/TextDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CodeEditCommands::CodeEditCommands
//
////////////////////////////////////////////////////////////////////////////////

CodeEditCommands::CodeEditCommands (IDebuggerPaneHost & host, const ICodeViews & views) :
    m_host  (host),
    m_views (views)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeEditCommands::GetSelection
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::pair<Word, Word>> CodeEditCommands::GetSelection (int view, int row) const
{
    const std::vector<DebuggerViewSnapshot::CodeLine>  & lines    = m_views.GetCodeLines (view);
    const std::vector<int>                             & selected = m_views.GetCodeList (view)->GetSelectedRows();
    std::vector<int>                                     rows     = std::ranges::find (selected, row) != selected.end() ? selected : std::vector<int> { row };
    std::optional<Word>                                  first;
    Word                                                 last     = 0;



    for (int each : rows)
    {
        int  index = m_views.GetCodeLineOfRow (view, each);



        if (index < 0)
        {
            continue;
        }

        const DebuggerViewSnapshot::CodeLine  & line  = lines[(size_t) index];
        Word                                    count = (Word) std::max<size_t> (1, (line.bytes.size() + 1) / 3);



        first = first.has_value() ? std::min (*first, line.address) : line.address;
        last  = std::max (last, (Word) (line.address + count - 1));
    }

    if (!first.has_value())
    {
        return std::nullopt;
    }

    return std::pair<Word, Word> { *first, std::max (last, *first) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeEditCommands::AddItems
//
////////////////////////////////////////////////////////////////////////////////

void CodeEditCommands::AddItems (int view, const DebuggerViewSnapshot::CodeLine & line, int row, PaneMenuItems & items)
{
    static constexpr std::pair<const wchar_t *, const char *>  kMarks[] =
    {
        { L"Mark as bytes", "DB" }, { L"Mark as words", "DW" }, { L"Mark as addresses", "DA" }, { L"Mark as text", "ASC" }, { L"Mark as floats", "DF" },
    };
    const DebuggerViewSnapshot  & snapshot = *m_host.GetLatestSnapshot();
    Word                          at       = line.address;
    Word                          count    = (Word) std::max<size_t> (1, (line.bytes.size() + 1) / 3);
    std::pair<Word, Word>         range    = GetSelection (view, row).value_or (std::pair<Word, Word> { at, (Word) (at + count - 1) });
    bool                          isCall   = line.instruction.starts_with ("JSR") && line.target.has_value();
    Word                          routine  = isCall ? *line.target : at;
    std::string                   name     = isCall ? line.shownOperand : line.label;
    auto                          bookmark = std::ranges::find_if (snapshot.bookmarks, [at] (const DebuggerViewSnapshot::WatchLine & each) { return each.address == at; });



    //  The routine is skipped by its symbol, not by an address written in hex.
    if (name.starts_with ('$') || name.empty() || !std::isalpha ((unsigned char) name[0]))
    {
        name.clear();
    }

    for (const auto & [label, directive] : kMarks)
    {
        items.push_back ({ label, [this, range, directive] { m_host.RunAction (DebuggerActions::GetMarkData (directive, range.first, range.second, m_host.GetMode())); } });
    }

    items.push_back ({ L"Clear data marking", [this, range] { m_host.RunAction (DebuggerActions::GetClearData (range.first, range.second, m_host.GetMode())); } });
    items.push_back ({ L"Add symbol here...", [this, at]    { AddSymbolAt (at); } });
    items.push_back ({ L"Never step into this routine", [this, name, routine] { m_host.RunAction (DebuggerActions::GetSkipRoutine (name, routine, m_host.GetMode())); } });

    if (bookmark != snapshot.bookmarks.end())
    {
        items.push_back ({ L"Remove bookmark", [this, id = bookmark->id] { m_host.RunAction (DebuggerActions::GetClearBookmark (id, m_host.GetMode())); } });
    }
    else
    {
        items.push_back ({ L"Add bookmark", [this, at] { m_host.RunAction (DebuggerActions::GetAddBookmark (at, m_host.GetMode())); } });
    }

    items.push_back ({ L"Start stopwatch here", [this, at] { m_stopwatchStart = at; } });

    if (m_stopwatchStart.has_value())
    {
        items.push_back ({ L"Stop stopwatch here", [this, at]
        {
            m_host.RunAction (DebuggerActions::GetStopwatch (*m_stopwatchStart, at, m_host.GetMode()));
            m_stopwatchStart.reset();
        } });
    }

    if (snapshot.isPaused)
    {
        items.push_back ({ L"Call subroutine here", [this, at] { m_host.RunAction (DebuggerActions::GetCallSubroutine (at, m_host.GetMode())); } });
        items.push_back ({ L"Edit instruction...",  [this, view, line]  { EditInstruction (view, line); } });
        items.push_back ({ L"Replace with NOPs",    [this, view, range] { ReplaceWithNops (view, range); } });
    }

    if (!m_undo.empty())
    {
        items.push_back ({ L"Undo " + m_undo.back().text, [this] { Undo (false); } });
    }

    if (!m_redo.empty())
    {
        items.push_back ({ L"Redo " + m_redo.back().text, [this] { Undo (true); } });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeEditCommands::TryGetBytes
//
////////////////////////////////////////////////////////////////////////////////

bool CodeEditCommands::TryGetBytes (int view, Word first, Word last, std::vector<Byte> & bytes) const
{
    uint32_t  next = first;



    bytes.clear();

    for (const DebuggerViewSnapshot::CodeLine & line : m_views.GetCodeLines (view))
    {
        std::vector<Byte>  lineBytes;



        if (line.address != next || next > last || !TextDialog::TryParseBytes (TextEncoding::NarrowToWide (line.bytes), lineBytes))
        {
            continue;
        }

        bytes.insert (bytes.end(), lineBytes.begin(), lineBytes.end());
        next += (uint32_t) lineBytes.size();
    }

    if (next <= last)
    {
        return false;
    }

    bytes.resize ((size_t) (last - first + 1));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeEditCommands::EditInstruction
//
//  A shorter instruction is padded with NOP to the line's end (FR-185), and
//  a longer one written only after Yes to a question listing the
//  instructions it covers, padded to the last one's end. ROM is not changed.
//  The write requires the bytes that were there, and the edit can be undone.
//
////////////////////////////////////////////////////////////////////////////////

void CodeEditCommands::EditInstruction (int view, const DebuggerViewSnapshot::CodeLine & line)
{
    static constexpr Byte          kNop     = 0xEA;
    const DebuggerViewSnapshot   * snapshot = m_host.GetLatestSnapshot();
    std::optional<std::wstring>    typed;
    std::vector<Byte>              bytes;
    std::vector<Byte>              before;
    std::vector<std::string>       covered;
    std::string                    error;
    uint32_t                       end      = 0;



    if (line.isRom)
    {
        m_host.ShowMessageBox (m_host.GetOwnerHwnd(), std::format (L"${:04X} is in ROM, which an instruction edit does not change.", line.address),
                               L"Edit instruction", MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (snapshot == nullptr || snapshot->instructionSet == nullptr)
    {
        return;
    }

    OpcodeTable    opcodes   (snapshot->instructionSet);
    LineAssembler  assembler (opcodes);



    typed = m_host.RunTextDialog (L"Edit instruction", L"Instruction", TextEncoding::NarrowToWide (line.instruction), [&assembler, &line] (const std::wstring & text, std::wstring & message)
    {
        std::vector<Byte>  trial;
        std::string        problem;
        bool               isOk    = assembler.TryAssemble (line.address, TextEncoding::WideToNarrow (text), trial, problem) == LineAssemblyStatus::Ok;



        message = TextEncoding::NarrowToWide (problem);
        return isOk;
    });

    if (!typed.has_value() || assembler.TryAssemble (line.address, TextEncoding::WideToNarrow (*typed), bytes, error) != LineAssemblyStatus::Ok)
    {
        return;
    }

    //  The lines the new instruction covers, its own first.
    end = line.address;

    for (const DebuggerViewSnapshot::CodeLine & each : m_views.GetCodeLines (view))
    {
        std::vector<Byte>  lineBytes;



        if (each.address != end || end >= (uint32_t) line.address + std::max<size_t> (bytes.size(), 1) || !TextDialog::TryParseBytes (TextEncoding::NarrowToWide (each.bytes), lineBytes))
        {
            continue;
        }

        if (each.address != line.address)
        {
            covered.push_back (each.instruction);
        }

        end += (uint32_t) lineBytes.size();
    }

    if (end > 0x10000 || end < (uint32_t) line.address + bytes.size())
    {
        return;
    }

    if (!covered.empty())
    {
        std::wstring  list;



        for (size_t i = 0; i < covered.size(); i++)
        {
            list += std::format (L"{}{}", (i == 0) ? L"" : (i + 1 == covered.size()) ? L" and " : L", ", TextEncoding::NarrowToWide (covered[i]));
        }

        if (m_host.ShowMessageBox (m_host.GetOwnerHwnd(), std::format (L"{} is longer than {}, and also overwrites {}. Write it?", *typed, TextEncoding::NarrowToWide (line.instruction), list),
                                   L"Edit instruction", MB_YESNO | MB_ICONQUESTION) != IDYES)
        {
            return;
        }
    }

    bytes.resize ((size_t) (end - line.address), kNop);

    if (!TryGetBytes (view, line.address, (Word) (end - 1), before))
    {
        return;
    }

    m_undo.push_back ({ line.address, before, bytes, std::format (L"edit at ${:04X}", line.address) });
    m_redo.clear();
    m_host.RunAction (DebuggerActions::GetPatch (line.address, bytes, m_host.GetMode(), before));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeEditCommands::ReplaceWithNops
//
//  Every byte of the selected lines (FR-186), undoably; nothing in ROM.
//
////////////////////////////////////////////////////////////////////////////////

void CodeEditCommands::ReplaceWithNops (int view, std::pair<Word, Word> range)
{
    static constexpr Byte  kNop  = 0xEA;
    std::vector<Byte>      before;
    std::vector<Byte>      after;
    bool                   isRom = false;



    for (const DebuggerViewSnapshot::CodeLine & line : m_views.GetCodeLines (view))
    {
        isRom = isRom || (line.isRom && line.address >= range.first && line.address <= range.second);
    }

    if (isRom)
    {
        m_host.ShowMessageBox (m_host.GetOwnerHwnd(), L"The selection is in ROM, which Replace with NOPs does not change.", L"Replace with NOPs", MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (!TryGetBytes (view, range.first, range.second, before))
    {
        return;
    }

    after.assign (before.size(), kNop);
    m_undo.push_back ({ range.first, before, after, std::format (L"NOPs at ${:04X}", range.first) });
    m_redo.clear();
    m_host.RunAction (DebuggerActions::GetPatch (range.first, after, m_host.GetMode(), before));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeEditCommands::Undo
//
//  The bytes put back, or written again, only over the bytes the step
//  wrote.
//
////////////////////////////////////////////////////////////////////////////////

void CodeEditCommands::Undo (bool redo)
{
    std::vector<Edit>  & from = redo ? m_redo : m_undo;
    std::vector<Edit>  & to   = redo ? m_undo : m_redo;



    if (from.empty())
    {
        return;
    }

    Edit  edit = from.back();



    from.pop_back();
    m_host.RunAction (redo ? DebuggerActions::GetPatch (edit.address, edit.after, m_host.GetMode(), edit.before)
                           : DebuggerActions::GetPatch (edit.address, edit.before, m_host.GetMode(), edit.after));
    to.push_back (std::move (edit));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeEditCommands::AddSymbolAt
//
////////////////////////////////////////////////////////////////////////////////

void CodeEditCommands::AddSymbolAt (Word address)
{
    std::optional<std::wstring>  typed;



    typed = m_host.RunTextDialog (L"Add symbol", L"Name", L"LOOP", [] (const std::wstring & text, std::wstring & error)
    {
        bool  isName = !text.empty() && iswalpha (text[0]) &&
                       std::ranges::all_of (text, [] (wchar_t ch) { return iswalnum (ch) || ch == L'_' || ch == L'.'; });



        error = isName ? std::wstring() : std::wstring (L"A symbol name starts with a letter and has letters, digits, _ and . after it.");
        return isName;
    });

    if (typed.has_value())
    {
        m_host.RunAction (DebuggerActions::GetAddSymbol (TextEncoding::WideToNarrow (*typed), address, m_host.GetMode()));
    }
}
