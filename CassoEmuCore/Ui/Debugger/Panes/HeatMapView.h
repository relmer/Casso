#pragma once

#include "Pch.h"

#include "Debugger/HeatAccessJump.h"
#include "Debugger/HeatMapOptions.h"
#include "Ui/Debugger/MemoryRegionKeys.h"
#include "Ui/Debugger/Panes/HeatMapCells.h"
#include "Ui/Debugger/Panes/HeatMapCellText.h"
#include "Ui/Debugger/Panes/HeatMapBands.h"
#include "Ui/Debugger/Panes/HeatMapControls.h"
#include "Ui/Debugger/Panes/HeatMapFrame.h"
#include "Ui/Debugger/Panes/HeatMapGrid.h"
#include "Ui/Debugger/Panes/HeatMapPainter.h"
#include "Ui/Debugger/Panes/HeatMapPicker.h"
#include "Ui/Debugger/Panes/HeatMapSections.h"
#include "Ui/Debugger/Panes/HeatMapZoomWidget.h"

struct BreakpointInfo;
class  HeatMapSymbols;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView
//
//  The 64 KB address space as a grid, one cell an address, each address drawn
//  in the color of what touched it, brighter the more it was touched. Code is
//  drawn in one color and data in two, reads and writes, so the program's
//  code stands apart from what it works on. Every cell is drawn apart from
//  its neighbors by a street of the page between them, and an untouched one
//  is a gray, not the page.
//
//  The rows fill the pane: each row has a power of two of addresses, from
//  kMinColumns to kMaxColumns, so a row always starts at a round address and
//  its label down the left reads as one. The count is the one that brings
//  the cells nearest the zoom's size, and the cells are then widened or
//  narrowed so the row spans the pane exactly, every column within a pixel of
//  the others; a row is always as tall as the zoom's cell. A row never runs
//  past the pane, so the map scrolls only up and down: at a larger zoom a row
//  has fewer addresses and the map has more rows.
//
//  Over the map a row of views: All shows everything, with code ahead of data
//  where an address is both; Code shows executes alone; Data shows reads and
//  writes alone; Changed shows the writes that changed their byte alone, in
//  a color of their own. In All and Data a read of RAM nothing had written
//  since power-on shows in its own color over the rest, as hot as such
//  reads were. Beside them the colors' key.
//
//  The wheel scrolls up and down, and Ctrl with the wheel zooms about the
//  mouse; a drag pans, and dragging the scrollbar moves the part in view. The
//  zoom widget in the corner displays the zoom and opens a slider
//  (HeatMapZoomWidget). A click on a cell displays its address in a memory
//  window.
//  While cells are small the cell the mouse picks is the busiest one near it,
//  so a lone hot byte is easy to land on. With Ctrl the click shows the
//  instruction that last wrote the address in the disassembly, with Shift
//  as well the one that last read it; with Ctrl and Alt it takes the
//  machine back through history to just after that write, or with Shift
//  as well that read.
//
//  The map shows one bank (HeatMapOptions::Bank), and the tip gives where
//  in it the cell is and, once the machine has looked them up, the cell's
//  last writer and reader.
//
//  Focused on a set of ranges, the map shows those alone, stacked in the
//  set's order, each under a header with its name and span. Each range's
//  rows follow the same rule, no more addresses to a row than the range
//  has, from the range's first address, and stretched to the pane; one
//  zoom sizes them all, and a new set starts at the largest that fits the
//  pane.
//
//  Over the heat, outlines mark the PC, the stack pointer's byte and every
//  enabled breakpoint, and an address with a symbol is tinted. Zoomed in far
//  enough, a cell shows its value, and further in, its opcode's form or its
//  bits, and an instruction's bytes merge into one cell.
//
//  The view is the pane's control, and the mouse and the keys go to it.
//  HeatMapCells has the data for each address and the text of its tip,
//  HeatMapGrid has the layout of the cells on the pane, and HeatMapPainter
//  has the drawing over them.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapView : public IDxuiControl
{
public:
    using Mode             = HeatMapCells::Mode;
    using Bank             = HeatMapCells::Bank;
    using Palette          = HeatMapCells::Palette;
    using BreakKind        = HeatMapCells::BreakKind;
    using Breakpoint       = HeatMapCells::Breakpoint;
    using CellKind         = HeatMapCells::CellKind;
    using TipLead          = HeatMapCells::TipLead;
    using Band             = HeatMapSections::Band;
    using Detail           = HeatMapCellText::Detail;
    using InstructionLines = HeatMapCellText::InstructionLines;

    //  Focusable, as every pane's content is, so after a click on the map
    //  its pane has the focus and is drawn with the accent.
    HeatMapView() { m_focusable = true; }

    //  Not copied: its parts have references to its members.
    HeatMapView             (const HeatMapView &) = delete;
    HeatMapView & operator= (const HeatMapView &) = delete;

    //  What a click on a cell asks for, by the keys held.
    enum class PickAction
    {
        ShowMemory,
        ShowWriter,
        ShowReader,
        RewindToWrite,
        RewindToRead,
    };

    static PickAction  GetPickAction (bool ctrl, bool shift, bool alt);

    //  The kind an enabled breakpoint on an address or a span is outlined
    //  as; none for a disabled one and one on no address (an opcode, a
    //  register, BRK, an interrupt).
    static std::optional<BreakKind>  GetBreakKind (const BreakpointInfo & info) { return HeatMapCells::GetBreakKind (info); }

    //  How far an address with a symbol is tinted toward the symbol color.
    static constexpr float  kSymbolTint  = HeatMapCells::kSymbolTint;

    //  The text drawn inside a cell large enough for it (HeatMapCellText).
    static constexpr float  kMinDetailFontDip = HeatMapCellText::kMinDetailFontDip;
    static constexpr int    kDetailPadDip     = HeatMapCellText::kDetailPadDip;
    static constexpr int    kValueChars       = HeatMapCellText::kValueChars;
    static constexpr int    kDetailChars      = HeatMapCellText::kDetailChars;
    static constexpr float  kAdvancePerEm     = HeatMapCellText::kAdvancePerEm;
    static constexpr float  kLinePerEm        = HeatMapCellText::kLinePerEm;

    static Detail        GetDetail          (long widthPx, long heightPx, float maxFontPx, float minFontPx, float padPx) { return HeatMapCellText::GetDetail (widthPx, heightPx, maxFontPx, minFontPx, padPx); }
    static std::wstring  GetDetailLine      (Byte value, bool isOpcode, const std::string & form)                       { return HeatMapCellText::GetDetailLine (value, isOpcode, form); }

    //  An instruction's length in bytes from its opcode's form ("LDA ....",
    //  3), and its text with the operand bytes filled in.
    static int       GetInstructionLength (const std::string & form) { return HeatMapCellText::GetInstructionLength (form); }
    std::wstring     GetInstructionText   (const std::string & form, Word cpu, const std::vector<Byte> & operands) const { return m_cells.GetInstructionText (form, cpu, operands); }
    static uint32_t  GetDetailTextColor   (uint32_t cell, uint32_t foreground, uint32_t background) { return HeatMapCellText::GetDetailTextColor (cell, foreground, background); }

    //  An instruction's text and bytes for each piece of a cell that wraps to
    //  the next row, by the number of cells in each piece: a first piece of
    //  one cell has the mnemonic and opcode byte, and the next the operand and
    //  its bytes; a longer first piece has all of it, and the rest are empty.
    static InstructionLines  SplitInstruction (const std::wstring & instruction, const std::wstring & bytes, const std::vector<long> & pieceCells)
    {
        return HeatMapCellText::SplitInstruction (instruction, bytes, pieceCells);
    }

    static constexpr int    kAddressCount   = HeatMapSections::kAddressCount;
    static constexpr int    kMinColumns     = HeatMapSections::kMinColumns;
    static constexpr int    kMaxColumns     = HeatMapSections::kMaxColumns;
    static constexpr int    kBarDip         = HeatMapGrid::kBarDip;
    static constexpr int    kTabDip         = 52;
    static constexpr int    kInsetDip       = HeatMapGrid::kInsetDip;
    static constexpr float  kLabelLine      = HeatMapGrid::kLabelLine;
    static constexpr int    kScrollbarDip   = HeatMapGrid::kScrollbarDip;
    static constexpr int    kModeCount      = 4;

    //  A cell's side, without the street of one pixel after it: three at 96
    //  dpi to start, from one to kMaxCellPx as the zoom goes.
    static constexpr int    kStreetPx       = HeatMapSections::kStreetPx;
    static constexpr int    kMaxCellPx      = HeatMapGrid::kMaxCellPx;

    //  The most a row's cells are widened to fill the pane before the row
    //  takes twice the addresses instead (HeatMapSections::kMaxStretch).
    static constexpr double kMaxStretch     = HeatMapSections::kMaxStretch;

    //  How far one notch of the wheel scrolls, rounded to whole rows.
    static constexpr int    kWheelStepDip   = 48;

    //  Below this side a cell is hard to land on, and over a cold cell the
    //  pick is the nearest hot cell in the same row within kSnapDip of the
    //  mouse.
    static constexpr int    kComfortCellDip = HeatMapPicker::kComfortCellDip;
    static constexpr int    kSnapDip        = HeatMapPicker::kSnapDip;

    //  How far the mouse moves with the button down before it pans.
    static constexpr int    kDragDip        = 4;

    //  How far from the cold gray toward its color the faintest touched
    //  address is drawn, so one touch is still visible; and the faintest
    //  read before written, which is rare and worth seeing at any count, so
    //  it is drawn nearly at full strength from the first read.
    static constexpr float  kFaintest          = HeatMapCells::kFaintest;
    static constexpr float  kUnwrittenFaintest = HeatMapCells::kUnwrittenFaintest;

    //  The levels, 0 for cold and 1 to 255 rising with the logarithm of the
    //  rate or the count, and the value level 255 stands for: accesses a
    //  second while fading, the busiest address's count while cumulative.
    void   SetLevels  (const std::vector<Byte> & execute, const std::vector<Byte> & read, const std::vector<Byte> & write);

    //  The same for the reads before written and the writes that changed
    //  their byte, drawn by the next SetLevels; and a mark for each byte the
    //  debugger edited, which the tip says.
    void  SetChannelLevels (const std::vector<Byte> & unwritten, const std::vector<Byte> & changed, const std::vector<Byte> & edited = {}) { m_cells.SetChannelLevels (unwritten, changed, edited); }
    void  SetTop           (double top) { m_cells.SetTop (top); }
    void  SetPalette       (const Palette & palette);
    void  SetMode          (Mode mode);
    Mode  GetMode          () const { return m_cells.GetOptions().view; }

    //  While the heat is rebuilt after a move through history, the bar says
    //  so.
    static constexpr const wchar_t * kpszRebuildingNote = HeatMapPainter::kpszRebuildingNote;


    void   SetRebuilding (bool isRebuilding) { m_painter.SetRebuilding (isRebuilding); }

    //  Where the note that the heat is being rebuilt goes: the room the
    //  window's strip leaves in the row of views.
    void   SetNoteRect   (const RECT & rect) { m_painter.SetNoteRect (rect); }
    bool   IsRebuilding  () const            { return m_painter.IsRebuilding(); }

    void                    SetOptions (const HeatMapOptions & options);
    const HeatMapOptions &  GetOptions () const { return m_cells.GetOptions(); }

    //  Which addresses were fetched as opcodes, nonzero where one was; empty
    //  for none known, when every byte executed is drawn as an opcode.
    void   SetOpcodes      (const std::vector<Byte> & opcodes) { m_cells.SetOpcodes (opcodes); }
    bool   IsOperand       (Word address) const                { return m_cells.IsOperand (address); }

    //  The bank's bytes, -1 where one is not known, and each opcode's form,
    //  for the cells large enough to show them.
    void   SetValues       (const std::vector<int16_t> & values) { m_cells.SetValues (values); }
    void   SetOpcodeForms  (std::shared_ptr<const std::vector<std::string>> forms) { m_cells.SetOpcodeForms (std::move (forms)); }

    //  The overlays: where in the bank the PC and the stack pointer are, the
    //  breakpoints, and the symbols, each on the CPU's addresses.
    void   SetCpuMarks     (std::optional<Word> pc, std::optional<Word> stack) { m_cells.SetCpuMarks (pc, stack); }
    void   SetBreakpoints  (std::vector<Breakpoint> breakpoints)                 { m_cells.SetBreakpoints (std::move (breakpoints)); }
    void   SetSymbols      (std::shared_ptr<const HeatMapSymbols> symbols);

    //  The labeled ranges: each address's MemoryRegionKeys key, empty for a
    //  bank with none; the card in each slot, which labels a slot's ranges;
    //  and the colors the ranges are outlined in and their bytes drawn in.
    //  With regions, the map is cut into sections at GetSectionStarts, each
    //  starting a row under a header with its label, in a rounded outline as
    //  a memory window outlines and labels its ranges. Any other labeled
    //  range, such as aux RAM, is outlined once cells are kRegionCellPx.
    static constexpr int  kRegionCellPx = HeatMapPainter::kRegionCellPx;

    //  The lowest contrast of an outline against the page, WCAG's minimum for
    //  a control's border; text's is DebuggerTextColors::s_kMinTextContrast.
    static constexpr float  kMinLineContrast = HeatMapPainter::kMinLineContrast;

    void      SetRegions      (const std::vector<uint16_t> & regions);
    void      SetSlotCards    (const MemoryRegionKeys::SlotCards & cards)   { m_cells.SetSlotCards (cards); }
    void      SetMachineId    (const std::string & machineId);
    void      SetRegionColors (const MemoryRegionKeys::Colors & colors)     { m_cells.SetRegionColors (colors); }
    uint16_t  GetRegionAt     (Word address) const { return m_cells.GetRegionAt (address); }

    //  The ink for a byte's text on its cell: ROM's and I/O's colors as a
    //  memory window draws those bytes, `plain` for any other, each moved
    //  until it reads on the cell.
    uint32_t  GetValueInk     (Word address, uint32_t cell, uint32_t plain) const { return m_cells.GetValueInk (address, cell, plain); }

    //  The symbol at an address in the current bank, empty for none.
    std::string  GetSymbolAt  (Word address) const { return m_cells.GetSymbolAt (address); }

    //  A cell's tip's first line, what the cell contains: an instruction run
    //  from it and its bytes, with any symbol before it, "LOOP: BPL $1075
    //  (10 FB)"; else its symbol or "Value" and its byte, "KBD: C1",
    //  "Value: A8". Empty for a byte not known.
    std::wstring  GetContentsLine (Word address) const { return m_cells.GetContentsLine (address); }

    //  The frame a cell's tip is drawn in: its range's label with the ROM
    //  behind it, and its color, 0 for the tip's. "RAM" for main RAM, and no
    //  frame in a bank with no regions.
    void   GetTipFrame     (Word address, std::wstring & label, uint32_t & argb) const { m_cells.GetTipFrame (address, label, argb); }

    //  The tag at the end of a cell's tip's first line: "PC", "SP" or both,
    //  in the PC's or the stack's color; empty for neither.
    void   GetTipTag       (Word address, std::wstring & tag, uint32_t & argb) const { m_cells.GetTipTag (address, tag, argb); }

    //  The outlines around an address's cell, outermost first: the PC's, the
    //  stack pointer's, then each breakpoint's on it.
    std::vector<uint32_t>  GetOutlineColors (Word address) const { return m_cells.GetOutlineColors (address); }

    //  Where the sections start (HeatMapSections::kSectionStarts), and every
    //  section start for a machine, in order: kSectionStarts; below them,
    //  where each built-in range in RAM begins and ends, so zero page, the
    //  stack and the text and hi-res pages are sections, and so is the RAM
    //  between them; and above them, each page where the machine's system
    //  ROM changes part, as $F800 starts the Monitor.
    static std::vector<Word>  GetSectionStarts (const std::string & machineId = {}) { return HeatMapSections::GetSectionStarts (machineId); }

    //  A section of RAM's label: the built-in range at an address, "Zero
    //  page", "Hi-res page 1", or "RAM" for none.
    static std::wstring       GetRamLabel     (Word address)      { return HeatMapSections::GetRamLabel (address); }

    //  A range's title and span, "Zero page  $0000-$00FF".
    static std::wstring       GetBandTitle    (const Band & band) { return HeatMapSections::GetBandTitle (band); }

    //  The ranges cut at GetSectionStarts, each piece a section. A range no
    //  start cuts is left whole, and a section only where `isAllMemory`.
    static std::vector<Band>  SplitAtSections (const std::vector<Band> & ranges, bool isAllMemory, const std::string & machineId = {})
    {
        return HeatMapSections::SplitAtSections (ranges, isAllMemory, machineId);
    }

    //  Whether the map is cut into sections now, and the ranges on the map.
    bool                       IsSplit  () const { return m_bands.IsSplit(); }
    const std::vector<Band> &  GetBands () const { return m_bands.GetAll(); }

    //  The height of a range's header, the room after a section's rows, and
    //  the note in place of an empty set.
    static constexpr int              kHeaderDip       = HeatMapGrid::kHeaderDip;

    //  A section's label's size, its header's height, and the room between
    //  its box and its cells on every side, which the label's line runs
    //  through at the top.
    static constexpr float            kSectionLabelDip  = HeatMapPainter::kSectionLabelDip;
    static constexpr int              kSectionPadDip    = HeatMapGrid::kSectionPadDip;
    static constexpr const wchar_t  * kpszNoRangesNote  = HeatMapPainter::kpszNoRangesNote;

    //  The ranges the map is focused on, which it then fits to the pane; or
    //  none of that, for all of memory.
    void   SetRanges   (std::vector<Band> ranges) { m_grid.SetRanges (std::move (ranges)); }
    void   ClearRanges ()                         { m_grid.ClearRanges(); }
    bool   HasRanges   () const                   { return m_bands.HasRanges(); }

    //  Where a range's header is drawn within the pane, as scrolled.
    RECT   GetHeaderRect (size_t range) const { return m_grid.GetHeaderRect (range); }

    //  The options changed by a press on the pane; a cell clicked.
    void   SetOnOptionsChanged (std::function<void()> fn)     { m_onOptionsChanged = std::move (fn); }
    void   SetOnPickAddress    (std::function<void(Word)> fn) { m_onPickAddress    = std::move (fn); }

    //  A click with Ctrl on a cell; the cell the mouse frames changed.
    void   SetOnPickAccess     (std::function<void(Word, PickAction)> fn)    { m_onPickAccess    = std::move (fn); }
    void   SetOnHoverChanged   (std::function<void(std::optional<Word>)> fn) { m_onHoverChanged  = std::move (fn); m_cells.SetLookingUp ((bool) m_onHoverChanged); }

    //  The bank the levels are of, and whether the machine has aux RAM, for
    //  the tip's location; the last writer and reader the machine looked up
    //  for a cell.
    void   SetShownBank        (Bank bank, bool hasAux) { m_cells.SetShownBank (bank, hasAux); }
    Bank   GetShownBank        () const                 { return m_cells.GetShownBank(); }
    void   SetHoverAccess      (std::shared_ptr<const HeatAccessHover> hover) { m_cells.SetHoverAccess (std::move (hover)); }

    //  The zoom, as a cell's height in pixels and the width a row's cells are
    //  stretched from; the addresses in a row and the rows; and how far the
    //  map is scrolled down in pixels from its top.
    int    GetCellPx    () const       { return m_grid.GetCellPx(); }
    int    GetColumns   () const       { return m_grid.GetColumns(); }
    int    GetRows      () const       { return m_bands.GetRows (m_grid.GetColumns()); }
    long   GetScrollTop () const       { return m_grid.GetScrollTop(); }
    void   ZoomAt       (POINT point, float notches) { m_grid.ZoomAt (point, notches); }
    void   ZoomIn       ()             { m_grid.ZoomIn(); }
    void   ZoomOut      ()             { m_grid.ZoomOut(); }
    void   ZoomTo       (int cellPx)   { m_grid.ZoomTo (cellPx); }
    void   ScrollBy     (int dy)       { m_grid.ScrollBy (dy); }
    void   ResetZoom    ()             { m_grid.ResetZoom(); }

    //  Zooms the map in all the way with an address centered and outlined, for
    //  the address box. The outline is drawn until another address replaces it.
    void                 ShowAddress     (Word address) { m_cells.SetFound (address); m_grid.ShowAddress (address); }
    std::optional<Word>  GetShownAddress () const       { return m_cells.GetFound(); }

    //  The cell size the map starts at, which the zoom's percentage is of;
    //  the zoom widget in the map's bottom-right corner; and its tip, for a
    //  point over its button.
    int                        GetStartCellPx  () const { return m_grid.GetStartCellPx(); }
    const HeatMapZoomWidget &  GetZoomWidget   () const { return m_controls.GetZoomWidget(); }
    bool                       TryGetZoomTipAt (POINT point, RECT & anchor, std::wstring & text) const { return m_controls.TryGetZoomTipAt (point, m_visible, anchor, text); }

    //  The addresses a row holds for a width at a pitch: the largest power of
    //  two from kMinColumns to kMaxColumns whose cells fit, or twice that when
    //  those would stretch past kMaxStretch and each cell is at least a pixel
    //  wide; one address when even one cell at the pitch is wider than the
    //  width.
    static int  GetColumnsFor (long widthPx, long pitchPx) { return HeatMapSections::GetColumnsFor (widthPx, pitchPx); }

    //  The addresses a row of a range holds, out of the map's columns: no
    //  more than the next power of two up from the range's size.
    static int  GetColumnsOf  (int columns, int count)     { return HeatMapSections::GetColumnsOf (columns, count); }

    //  Whether the map is taller than its area, so it scrolls and has a
    //  scrollbar; and where the scrollbar is drawn, empty without one.
    bool   HasVerticalScroll  () const { return m_grid.HasVerticalScroll(); }
    RECT   GetVerticalBarRect () const { return m_grid.GetVerticalBarRect(); }

    //  The map as drawn, a pixel each, the size of the map's area, drawn again
    //  first if anything changed it since; and the color one address is drawn in.
    const std::vector<uint32_t> &  GetPixels    ()                   { return m_frame.GetPixels (m_visible); }
    uint32_t                       GetCellColor (Word address) const { return m_cells.GetCellColor (address); }

    static uint32_t      GetColor     (Mode mode, Byte execute, Byte read, Byte write, const Palette & palette) { return HeatMapCells::GetColor (mode, execute, read, write, palette); }

    //  The same with Blend mixing the kinds that touched it instead of
    //  showing the hottest.
    static uint32_t      GetColor     (Mode mode, Byte execute, Byte read, Byte write, const Palette & palette, bool isBlend)
    {
        return HeatMapCells::GetColor (mode, execute, read, write, palette, isBlend);
    }

    //  An address's color with the reads before written and the changes
    //  drawn as the mode shows them, from its color without them.
    static uint32_t      GetChannelColor (Mode mode, Byte unwritten, Byte changed, uint32_t color, const Palette & palette)
    {
        return HeatMapCells::GetChannelColor (mode, unwritten, changed, color, palette);
    }

    static std::wstring  GetModeLabel    (Mode mode) { return HeatMapCells::GetModeLabel (mode); }

    //  The map's area within the pane: the part of the rows in view, without
    //  the scrollbar; and an address's cell within the pane, as scrolled.
    RECT                 GetMapRect  () const { return m_grid.GetMapRect(); }

    //  The row of views, where the window puts the strip that picks the
    //  view, Blend, the set of ranges and the bank shown; empty while it has
    //  no room.
    RECT  GetViewRowFreeRect () const             { return m_grid.GetViewRowFreeRect (m_visible); }
    RECT  GetCellRect        (Word address) const { return m_grid.GetCellRect (address); }

    //  The address under a point in the map, a street counting as the cell
    //  before it; the address the mouse picks there, which may be a busier one
    //  near it; the mode under a point in the bar.
    std::optional<Word>  GetAddressAt (POINT point) const { return m_grid.GetAddressAt (point); }
    std::optional<Word>  GetPickAt    (POINT point) const { return m_picker.GetPickAt (point); }

    std::optional<Word>  GetHover     () const { return m_grid.GetHover(); }
    bool                 IsPressed    () const;

    //  Whether a point is over the map or its scrollbar.
    bool                 IsOverMap    (POINT point) const { return m_grid.IsOverMap (point); }

    //  The tip for a point on the map: the cell it picks, framed, and its
    //  address and what touched it, "$C65E  executed 120/s, read 3/s"; false
    //  off the map and while a button is down. The tip's first line is the
    //  meaning of the cell's color (GetTipLead) and `swatches` has the cell's
    //  color for that line, zero for the rest. `gradient` and `mark` are the
    //  cell's scale and its level on it (TryGetCellScale), or empty and -1.
    bool                 TryGetTipAt  (POINT point, RECT & anchor, std::wstring & text, std::vector<uint32_t> & swatches) const;
    bool                 TryGetTipAt  (POINT point, RECT & anchor, std::wstring & text, std::vector<uint32_t> & swatches,
                                       std::vector<uint32_t> & gradient, float & mark) const;

    static CellKind  GetCellKind (Mode mode, Byte execute, Byte read, Byte write, Byte unwritten, Byte changed, bool isOperand, bool isBlend)
    {
        return HeatMapCells::GetCellKind (mode, execute, read, write, unwritten, changed, isOperand, isBlend);
    }

    CellKind         GetCellKind (Word address) const { return m_cells.GetCellKind (address); }

    TipLead              GetTipLead   (Word address) const { return m_cells.GetTipLead (address); }

    //  The scale a cell's color is from, faintest to full, as the legend's
    //  Ramp swatch draws it, and where on it the cell's level lies, 0 to 1;
    //  false for an untouched cell and a Blend mix, which have no one scale.
    bool                   TryGetCellScale (Word address, std::vector<uint32_t> & gradient, float & mark) const { return m_cells.TryGetCellScale (address, gradient, mark); }
    std::vector<uint32_t>  GetScaleStops   (CellKind kind) const { return m_cells.GetScaleStops (kind); }

    //  Whether the tip for a point is whole: the last writer and reader of
    //  the cell it picks are in, or nothing looks them up. A tip shown before
    //  them gains two lines when they arrive.
    bool          IsTipComplete (POINT point) const;
    std::wstring  GetTipText    (Word address) const { return m_cells.GetTipText (address); }

    //  The widening of a scrollbar under the mouse, carried out over frames.
    bool                 TickScrollbars (int64_t nowMs) { return m_controls.Tick (nowMs); }

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent & ev) override;
    void                OnFocusChanged    (bool focused) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Heat map"; }

private:
    bool                                OnPress       (const DxuiMouseEvent & ev);
    bool                                OnRelease     (const DxuiMouseEvent & ev);
    bool                                OnDragOrHover (const DxuiMouseEvent & ev);
    bool                                OnWheel       (const DxuiMouseEvent & ev);
    bool                                OnZoomWidget  (const DxuiMouseEvent & ev);
    void                                SetHover      (const std::optional<HeatMapBands::Place> & hover);
    std::optional<HeatMapBands::Place>  GetKeyTarget  (const DxuiKeyEvent & ev) const;

    HeatMapCells               m_cells;
    HeatMapBands               m_bands;
    HeatMapControls            m_controls;
    HeatMapGrid                m_grid         { m_cells, m_bands, m_controls };
    HeatMapFrame               m_frame        { m_grid, m_bands, m_cells };
    HeatMapPicker              m_picker       { m_grid, m_bands, m_cells };
    HeatMapPainter             m_painter      { m_grid, m_bands, m_picker, m_cells };
    std::optional<POINT>       m_press;
    long                       m_pressTop     = 0;
    bool                       m_isDragging   = false;
    std::function<void()>      m_onOptionsChanged;
    std::function<void(Word)>  m_onPickAddress;

    std::function<void(Word, PickAction)>     m_onPickAccess;
    std::function<void(std::optional<Word>)>  m_onHoverChanged;
};
