#pragma once

#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/DisassemblyOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViewFind
//
//  A disassembly view's find. While the machine is paused, it searches every
//  line of the address space: the address, bytes, label and instruction
//  columns that are visible, as the view writes them. A match is brought into
//  view and selected there. The annotation column isn't searched; its text
//  exists for the lines on screen alone.
//  While the machine runs, the view's rows are what is searched, as the list
//  searches them.
//
////////////////////////////////////////////////////////////////////////////////

class CodeViewFind : public IDxuiFindTarget
{
public:
    using  Lines    = std::vector<DebuggerViewSnapshot::CodeLine>;
    using  Visible  = std::array<bool, DisassemblyCells::kColumnCount>;
    using  LinesFn  = std::function<std::shared_ptr<const Lines> ()>;
    using  WhereFn  = std::function<std::optional<Word> (bool isTop)>;
    using  SelectFn = std::function<void (Word address)>;
    using  SourceFn = std::function<bool (int fileId, int line, std::wstring & text)>;

    //  lines returns the paused machine's lines, or none while it runs;
    //  where returns the selected line's address, or the view's top line's
    //  with isTop set; select brings a match's line into view; and source
    //  returns a source line's text.
    CodeViewFind (DxuiListView & list, const DisassemblyOptions & options, LinesFn lines, WhereFn where, SelectFn select, SourceFn source);

    //  The lines whose searched columns match, in address order. While the
    //  Source option is on, the text of the source line above a line's
    //  instruction is searched too, a match being at its first instruction.
    static std::vector<Word>  FindLines (const Lines & lines, const DisassemblyOptions & options, const Visible & visible, const SourceFn & source,
                                         const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex);

    IDxuiControl    * GetFindControl          () override;
    RECT              GetFindBounds           () const override;
    bool              IsFindVisible           () const override;
    std::wstring      GetSelectionText        () const override;
    DxuiFindResult    SelectMatch             (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, bool forward, int & outIndex, int & outCount) override;
    DxuiFindResult    SelectMatchInView       (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, int & outIndex, int & outCount) override;
    void              SetFindScopeToSelection () override;
    void              ClearFindScope          () override;
    void              ClearFindHighlights     () override;
    void              SetFindColors           (uint32_t matchArgb, uint32_t currentArgb) override;

private:
    Visible            GetVisible () const;
    std::vector<Word>  Search     (const Lines & lines, const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex);
    void               SetMarks   (const std::vector<Word> & found, int chosen);

    DxuiListView               & m_list;
    const DisassemblyOptions   & m_options;
    LinesFn                      m_lines;
    WhereFn                      m_where;
    SelectFn                     m_select;
    SourceFn                     m_source;
};
