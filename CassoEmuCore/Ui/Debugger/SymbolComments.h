#pragma once

#include "Pch.h"
#include "Debugger/Source/SourceService.h"
#include "Ui/Debugger/DebuggerViewState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolComments
//
//  The comment on a label's defining line in its source file, for the tips
//  that have the label's description. The defining line is the first line
//  whose code starts at the label's address, or the line above it when that
//  line has the label alone; its comment is the text after a semicolon that
//  is not in quotes. A file is read once, by the lookup the source pane uses.
//
////////////////////////////////////////////////////////////////////////////////

class SymbolComments
{
public:
    using FindFn = std::function<SourceLookup (const DebugSourceFile & record, const std::wstring & debugFilePath, const std::string & programKey)>;

    //  The source state of the latest snapshot; a different line table drops
    //  what was read for the old one.
    void         SetSource (const DebuggerViewSnapshot::SourceState & state);

    //  The comment for the label at an address, empty with none.
    std::string  GetComment (Word address, const std::string & name, const FindFn & find);

    //  A source line's comment, trimmed; empty with none.
    static std::string  GetLineComment (const std::string & line);

    //  The defining line's place, (file, line), from the lines and their
    //  first addresses; none when no line starts at the address. Of several
    //  lines starting there, the first isLabelLine accepts, else the earliest.
    static std::optional<std::pair<int, int>>  FindDefiningLine (const std::map<std::pair<int, int>, Word> & lineAddresses, Word address,
                                                                 const std::function<bool (int fileId, int line)> & isLabelLine = {});

    //  The line defining the label at an address: of the lines starting
    //  there, as a macro's call and the lines of its body all do, the one
    //  with a label in its first column. None with no source loaded.
    std::optional<std::pair<int, int>>  FindLabelLine (Word address, const FindFn & find);

    //  Whether a source line starts with a label: anything but a blank or a
    //  comment in its first column.
    static bool  IsLabelLine (const std::string & text);

    //  The comment of the defining line, or of the line above it when that
    //  line is the label alone.
    static std::string  GetDefiningComment (const std::vector<std::string> & lines, int line, const std::string & name);

private:
    const std::vector<std::string> *  GetLines (int fileId, const FindFn & find);

    std::optional<DebuggerViewSnapshot::SourceState>  m_state;
    std::map<int, std::vector<std::string>>           m_files;
    std::map<Word, std::string>                       m_comments;
};