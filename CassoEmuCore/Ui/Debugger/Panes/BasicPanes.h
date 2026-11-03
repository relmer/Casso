#pragma once

#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/Panes/HeatMapView.h"
#include "Ui/Debugger/SourceSyntax.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramPane
//
//  The Applesoft program as LIST prints it (FR-173), read from memory, with
//  the line being run on the PC's row color and its statement in full color
//  and the rest of the line muted, the PC's arrow on the row the statement
//  starts on, and a breakpoint's dot on each line with a BPBASIC
//  breakpoint. Activating a row, by a double-click or Enter, sets a line
//  breakpoint there or clears the one there.
//
//  The window owns the list; the pane has a pointer to it.
//
////////////////////////////////////////////////////////////////////////////////

class BasicProgramPane
{
public:
    using ToggleFn = std::function<void (Word line)>;

    //  The icons and colors the disassembly marks a line with, so the two
    //  panes mark one alike.
    struct Style
    {
        std::shared_ptr<const DxuiIconImage>  pcIcon;
        std::shared_ptr<const DxuiIconImage>  enabledIcon;
        std::shared_ptr<const DxuiIconImage>  disabledIcon;
        std::shared_ptr<const DxuiIconImage>  pcOnEnabledIcon;
        std::shared_ptr<const DxuiIconImage>  pcOnDisabledIcon;
        uint32_t                              pcRowArgb = 0;
        SourceSyntax::Colors                  syntax;

        bool operator== (const Style & other) const = default;
    };

    //  One row as LIST prints it: its line, whether it is the line's first,
    //  and where each of the line's pieces on it begins, by piece.
    struct Row
    {
        std::wstring                      text;
        size_t                            lineIndex = 0;
        bool                              isFirst   = true;
        std::vector<std::pair<int, int>>  pieces;   // piece index, offset in text
    };

    static constexpr size_t  kGutterColumn  = 0;
    static constexpr size_t  kHeatColumn    = 1;
    static constexpr size_t  kListingColumn = 2;
    static constexpr size_t  kValuesColumn  = 3;

    //  The width of a run of text in the listing's face, in pixels.
    using MeasureFn = std::function<float (const std::wstring & text)>;

    BasicProgramPane (DxuiListView * list, ToggleFn toggle);

    DxuiListView *  GetList () const { return m_list; }

    void  Configure (int gutterDip);
    void  SetStyle  (const Style & style) { m_style = style; }

    //  The heat column's levels and colors while Show heat is on; the column
    //  is visible while there are colors.
    void  SetHeat   (std::shared_ptr<const DebuggerViewSnapshot::HeatColumnLevels> levels, const std::optional<HeatMapView::Palette> & palette);
    void  Apply     (const std::optional<DebuggerViewSnapshot::BasicState> & basic, const std::vector<DebuggerViewSnapshot::BreakpointLine> & breakpoints);

    //  Scrolls to a line's first row and selects it.
    void  ShowLine  (Word line);

    //  The line a row of the listing is part of, none for a note's row.
    std::optional<Word>  GetLineOfRow (int row) const { return (row >= 0 && (size_t) row < m_rowLines.size()) ? std::optional<Word> (m_rowLines[(size_t) row]) : std::nullopt; }

    //  The width the listing wraps at, in pixels, and how to measure it; the
    //  rows are made again with the next Apply. No width wraps as LIST does.
    void   SetWrap        (float widthPx, MeasureFn measure) { m_wrapPx = widthPx; m_measure = std::move (measure); }
    float  GetWrapPx      () const { return m_wrapPx; }

    //  The rows LIST prints, with each piece's place.
    static std::vector<Row>  GetRows (const std::vector<BasicLine> & lines);

    //  The rows wrapped at a width instead: a new row, indented as LIST
    //  indents one, where the next piece would pass the width.
    static std::vector<Row>  GetRows (const std::vector<BasicLine> & lines, float widthPx, const MeasureFn & measure);

    //  A line's values column: each variable the line uses and its value,
    //  and for the line the machine stopped at before, a value that changed
    //  as old -> new. The before parameter is the program at that stop.
    static std::wstring  GetValuesText (const BasicLine & line, const DebuggerViewSnapshot::BasicState & basic,
                                        const DebuggerViewSnapshot::BasicState * before);

    //  The tip over a line's text: each variable it uses and its value, a
    //  line each; empty for a line with no variables.
    static std::wstring  GetValuesTip (const BasicLine & line, const DebuggerViewSnapshot::BasicState & basic);

    //  The pieces of the statement that the piece at `piece` is in, first
    //  and one past the last: a statement ends at a colon outside quotes,
    //  and a REM runs to the end of the line.
    static std::pair<int, int>  GetStatement (const BasicLine & line, int piece);

    //  Each row's cells: the gutter's icon, the heat strip over the line's
    //  tokens on its first row when there are levels, and the text, colored.
    static std::vector<std::vector<DxuiListView::Cell>>  GetCells (const DebuggerViewSnapshot::BasicState                & basic,
                                                                    const std::vector<Row>                                 & rows,
                                                                    const std::vector<DebuggerViewSnapshot::BreakpointLine> & breakpoints,
                                                                    const Style                                            & style,
                                                                    const DebuggerViewSnapshot::HeatColumnLevels           * heat    = nullptr,
                                                                    const HeatMapView::Palette                             * palette = nullptr);

    //  The note in place of the listing, empty when there is a listing.
    static std::wstring  GetNote (const std::optional<DebuggerViewSnapshot::BasicState> & basic);

private:
    static void  AddSyntaxColors (const BasicLine & line, const Row & row, const SourceSyntax::Colors & syntax, DxuiListView::Cell & cell);

    DxuiListView      * m_list = nullptr;
    ToggleFn            m_toggle;
    Style               m_style;
    std::vector<Row>    m_rows;
    std::vector<Word>   m_rowLines;

    //  The heat column's levels and colors, none while it is hidden.
    std::shared_ptr<const DebuggerViewSnapshot::HeatColumnLevels>  m_heat;
    std::optional<HeatMapView::Palette>                            m_heatPalette;

    float                                                          m_wrapPx = 0.0f;
    MeasureFn                                                      m_measure;

    //  The program at the stop on screen and at the one before, for the
    //  values a line changed.
    std::optional<DebuggerViewSnapshot::BasicState>                m_stop;
    std::optional<DebuggerViewSnapshot::BasicState>                m_stopBefore;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BasicWatches
//
//  The BASIC variables the Watches pane lists while the machine is stopped
//  in a running Applesoft program (FR-176): the ones the statement being run
//  uses and every simple variable and array element. The ones the user adds
//  are watches in the session. Applesoft reads only a name's first two
//  characters, so the rest are dimmed.
//
////////////////////////////////////////////////////////////////////////////////

class BasicWatches
{
public:
    //  One variable or element: its name as written and its value as PRINT
    //  writes it, a string in quotes.
    struct Item
    {
        std::string  name;
        std::string  value;
    };

    //  Whether the machine is stopped in a running program, so the values can
    //  be read.
    static bool  IsStoppedInProgram (const std::optional<DebuggerViewSnapshot::BasicState> & basic);

    //  The value, as Applesoft reads it: 0 or "" for a variable not set yet;
    //  none for text that is not a variable or an element out of range.
    static std::optional<std::string>  TryGetValue (const DebuggerViewSnapshot::BasicState & basic, const std::string & text);

    //  The simple variables a line's pieces from the first index to the one before the last
    //  use, each once, in the order they appear.
    static std::vector<std::string>  GetNames (const BasicLine & line, int first, int last);

    //  The simple variables the statement being run uses, each once, in the
    //  order they appear.
    static std::vector<Item>  GetStatementItems (const DebuggerViewSnapshot::BasicState & basic);

    //  Every simple variable, then every array element.
    static std::vector<Item>  GetAllItems (const DebuggerViewSnapshot::BasicState & basic);

    //  An element's name: the array's, then its subscripts, the first varying
    //  fastest.
    static std::string  GetElementName (const BasicArray & array, size_t index);

    //  A name's cell, the letters past the second dimmed. Where another row's
    //  label is the same name, (BASIC) follows it.
    static DxuiListView::Cell  MakeNameCell (const std::string & name, const std::set<std::string> & otherLabels = {});

private:
    static std::string  Quote (BasicType type, const std::string & value);
};





////////////////////////////////////////////////////////////////////////////////
//
//  BasicAutoWatches
//
//  The BASIC variables among the automatic watches at each stop: the
//  statement being run's, then those of the statement the stop before was
//  at that this one does not use, as Visual Studio's Autos list the line
//  before as well. A new stop has a cycle count different from the last
//  one's. A value that differs from the stop before's is changed.
//
////////////////////////////////////////////////////////////////////////////////

class BasicAutoWatches
{
public:
    struct Row
    {
        BasicWatches::Item  item;
        bool                isPrevious = false;     // the statement before's, not the one being run's
        bool                isChanged  = false;
    };

    std::vector<Row>  Update (const DebuggerViewSnapshot::BasicState & basic, uint64_t cycles);

private:
    uint64_t                            m_cycles = UINT64_MAX;
    std::vector<std::string>            m_names;            // the statement being run's, at this stop
    std::vector<std::string>            m_previous;         // the statement the stop before was at
    std::map<std::string, std::string>  m_values;           // at this stop
    std::map<std::string, std::string>  m_lastValues;       // at the stop before
};
