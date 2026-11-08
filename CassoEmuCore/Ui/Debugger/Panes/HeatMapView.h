#pragma once

#include "Pch.h"

#include "Debugger/HeatAccessJump.h"
#include "Debugger/HeatMapOptions.h"
#include "Ui/Debugger/MemoryRegionKeys.h"
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
//  The rows fill the pane: each row holds a power of two of addresses, from
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
//  holds, from the range's first address, and stretched to the pane; one
//  zoom sizes them all, and a new set starts at the largest that fits the
//  pane.
//
//  Over the heat, outlines mark the PC, the stack pointer's byte and every
//  enabled breakpoint, and an address with a symbol is tinted. Zoomed in far
//  enough, a cell shows its value, and further in, its opcode's form or its
//  bits, and an instruction's bytes merge into one cell.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapView : public IDxuiControl
{
public:
    using Mode = HeatMapOptions::View;
    using Bank = HeatMapOptions::Bank;

    //  Focusable, as every pane's content is, so after a click on the map
    //  its pane has the focus and is drawn with the accent.
    HeatMapView() { m_focusable = true; }

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

    //  The heat's colors, then the overlays': the PC's outline and the stack
    //  pointer's, a breakpoint's and a read or write watchpoint's, the tint
    //  of an address with a symbol, and Blend's color for an address both run
    //  as code and written.
    struct Palette
    {
        uint32_t  background    = 0;
        uint32_t  cold          = 0;
        uint32_t  execute       = 0;
        uint32_t  read          = 0;
        uint32_t  write         = 0;
        uint32_t  pc            = 0;
        uint32_t  stack         = 0;
        uint32_t  breakpoint    = 0;
        uint32_t  readWatch     = 0;
        uint32_t  writeWatch    = 0;
        uint32_t  symbol        = 0;
        uint32_t  selfModifying = 0;
        uint32_t  unwritten     = 0;
        uint32_t  changed       = 0;

        bool operator== (const Palette & other) const = default;
    };

    //  A breakpoint the map outlines: on running an address, or a watchpoint
    //  on reading or writing a span, or both.
    enum class BreakKind
    {
        Execute,
        Read,
        Write,
        ReadWrite,
    };

    struct Breakpoint
    {
        int        id    = 0;
        Word       first = 0;
        Word       last  = 0;
        BreakKind  kind  = BreakKind::Execute;

        bool operator== (const Breakpoint & other) const = default;
    };

    //  The kind an enabled breakpoint on an address or a span is outlined
    //  as; none for a disabled one and one on no address (an opcode, a
    //  register, BRK, an interrupt).
    static std::optional<BreakKind>  GetBreakKind (const BreakpointInfo & info);

    //  How far an address with a symbol is tinted toward the symbol color.
    static constexpr float  kSymbolTint  = 0.25f;

    //  What a cell large enough shows inside it: its value in hex on one
    //  line, and on a second the opcode's form when it was run as one, or
    //  else the value in binary. A line's text is in the monospace face, as
    //  large as the cell lets it be up to the theme's size, and shows only
    //  where that is at least kMinDetailFontDip; the second line needs room
    //  for kDetailChars. A character is taken as kAdvancePerEm of the size
    //  across and a line kLinePerEm of it down.
    struct Detail
    {
        int    lines  = 0;
        float  fontPx = 0.0f;
    };

    static constexpr float  kMinDetailFontDip = 9.0f;
    static constexpr int    kDetailPadDip     = 2;
    static constexpr int    kValueChars       = 2;
    static constexpr int    kDetailChars      = 10;
    static constexpr float  kAdvancePerEm     = 0.6f;
    static constexpr float  kLinePerEm        = 1.25f;

    static Detail        GetDetail          (long widthPx, long heightPx, float maxFontPx, float minFontPx, float padPx);
    static std::wstring  GetDetailLine      (Byte value, bool isOpcode, const std::string & form);

    //  An instruction's length in bytes from its opcode's form ("LDA ....",
    //  3), and its text with the operand bytes filled in.
    static int           GetInstructionLength (const std::string & form);
    std::wstring         GetInstructionText   (const std::string & form, Word cpu, const std::vector<Byte> & operands) const;
    static uint32_t      GetDetailTextColor (uint32_t cell, uint32_t foreground, uint32_t background);

    //  An instruction's text and bytes for each piece of a cell that wraps to
    //  the next row, by the number of cells in each piece: a first piece of
    //  one cell has the mnemonic and opcode byte, and the next the operand and
    //  its bytes; a longer first piece has all of it, and the rest are empty.
    using InstructionLines = std::vector<std::pair<std::wstring, std::wstring>>;

    static InstructionLines  SplitInstruction (const std::wstring & instruction, const std::wstring & bytes, const std::vector<long> & pieceCells);

    static constexpr int    kAddressCount   = 0x10000;
    static constexpr int    kMinColumns     = 1;
    static constexpr int    kMaxColumns     = 1024;
    static constexpr int    kBarDip         = 22;      // the row of views over the map
    static constexpr int    kTabDip         = 52;
    static constexpr int    kGutterDip      = 44;      // the row labels' column, until a paint measures them
    static constexpr int    kInsetDip       = 8;       // right of the map and below it
    static constexpr float  kLabelLine      = 1.4f;    // a row label's line, in its font's sizes
    static constexpr int    kScrollbarDip   = 10;
    static constexpr int    kModeCount      = 4;

    //  A cell's side, without the street of one pixel after it: three at 96
    //  dpi to start, from one to kMaxCellPx as the zoom goes.
    static constexpr int    kStreetPx       = 1;
    static constexpr int    kDefaultCellDip = 3;
    static constexpr int    kMaxCellPx      = 64;
    static constexpr float  kZoomPerNotch   = 1.5f;

    //  The most a row's cells are widened to fill the pane before the row
    //  takes twice the addresses instead, each a little narrower than the
    //  zoom's: a column stays between three quarters and one and a half times
    //  the zoom's width, except where kMaxColumns or a one-pixel cell stop it,
    //  or where a single cell at the zoom's width is wider than the pane.
    static constexpr double kMaxStretch     = 1.5;

    //  How far one notch of the wheel scrolls, rounded to whole rows.
    static constexpr int    kWheelStepDip   = 48;

    //  Below this side a cell is hard to land on, and over a cold cell the
    //  pick is the nearest hot cell in the same row within kSnapDip of the
    //  mouse.
    static constexpr int    kComfortCellDip = 8;
    static constexpr int    kSnapDip        = 4;

    //  How far the mouse moves with the button down before it pans.
    static constexpr int    kDragDip        = 4;

    //  How far from the cold gray toward its color the faintest touched
    //  address is drawn, so one touch is still visible; and the faintest
    //  read before written, which is rare and worth seeing at any count, so
    //  it is drawn nearly at full strength from the first read.
    static constexpr float  kFaintest          = 0.3f;
    static constexpr float  kUnwrittenFaintest = 0.8f;

    //  The levels, 0 for cold and 1 to 255 rising with the logarithm of the
    //  rate or the count, and the value level 255 stands for: accesses a
    //  second while fading, the busiest address's count while cumulative.
    void   SetLevels  (const std::vector<Byte> & execute, const std::vector<Byte> & read, const std::vector<Byte> & write);

    //  The same for the reads before written and the writes that changed
    //  their byte, drawn by the next SetLevels; and a mark for each byte the
    //  debugger edited, which the tip says.
    void   SetChannelLevels (const std::vector<Byte> & unwritten, const std::vector<Byte> & changed, const std::vector<Byte> & edited = {});
    void   SetTop     (double top) { m_top = top; }
    void   SetPalette (const Palette & palette);
    void   SetMode    (Mode mode);
    Mode   GetMode    () const { return m_options.view; }

    //  While the heat is rebuilt after a move through history, the bar says
    //  so.
    static constexpr const wchar_t * kpszRebuildingNote = L"Rebuilding";


    void   SetRebuilding (bool isRebuilding) { m_isRebuilding = isRebuilding; }

    //  Where the note that the heat is being rebuilt goes: the room the
    //  window's strip leaves in the row of views.
    void   SetNoteRect   (const RECT & rect) { m_noteRect = rect; }
    bool   IsRebuilding  () const            { return m_isRebuilding; }

    void                    SetOptions (const HeatMapOptions & options);
    const HeatMapOptions &  GetOptions () const { return m_options; }

    //  Which addresses were fetched as opcodes, nonzero where one was; empty
    //  for none known, when every byte executed is drawn as an opcode.
    void   SetOpcodes      (const std::vector<Byte> & opcodes);
    bool   IsOperand       (Word address) const;

    //  The bank's bytes, -1 where one is not known, and each opcode's form,
    //  for the cells large enough to show them.
    void   SetValues       (const std::vector<int16_t> & values) { m_values = values; }
    void   SetOpcodeForms  (std::shared_ptr<const std::vector<std::string>> forms) { m_opcodeForms = std::move (forms); }

    //  The overlays: where in the bank the PC and the stack pointer are, the
    //  breakpoints, and the symbols, each on the CPU's addresses.
    void   SetCpuMarks     (std::optional<Word> pc, std::optional<Word> stack) { m_pc = pc; m_stack = stack; }
    void   SetBreakpoints  (std::vector<Breakpoint> breakpoints);
    void   SetSymbols      (std::shared_ptr<const HeatMapSymbols> symbols);

    //  The labeled ranges: each address's MemoryRegionKeys key, empty for a
    //  bank with none; the card in each slot, which labels a slot's ranges;
    //  and the colors the ranges are outlined in and their bytes drawn in.
    //  With regions, the map is cut into sections at GetSectionStarts, each
    //  starting a row under a header with its label, in a rounded outline as
    //  a memory window outlines and labels its ranges. Any other labeled
    //  range, such as aux RAM, is outlined once cells are kRegionCellPx.
    static constexpr int  kRegionCellPx = 4;

    //  The lowest contrast of an outline against the page, WCAG's minimum for
    //  a control's border; text's is DebuggerTextColors::s_kMinTextContrast.
    static constexpr float  kMinLineContrast = 3.0f;

    void      SetRegions      (const std::vector<uint16_t> & regions);
    void      SetSlotCards    (const MemoryRegionKeys::SlotCards & cards)   { m_slotCards = cards; }
    void      SetMachineId    (const std::string & machineId);
    void      SetRegionColors (const MemoryRegionKeys::Colors & colors)     { m_regionColors = colors; }
    uint16_t  GetRegionAt     (Word address) const { return (m_regions.size() == (size_t) kAddressCount) ? m_regions[address] : (uint16_t) 0; }

    //  The ink for a byte's text on its cell: ROM's and I/O's colors as a
    //  memory window draws those bytes, `plain` for any other, each moved
    //  until it reads on the cell.
    uint32_t  GetValueInk     (Word address, uint32_t cell, uint32_t plain) const;

    //  The symbol at an address in the current bank, empty for none.
    std::string  GetSymbolAt  (Word address) const;

    //  A cell's tip's first line, what the cell contains: an instruction run
    //  from it and its bytes, with any symbol before it, "LOOP: BPL $1075
    //  (10 FB)"; else its symbol or "Value" and its byte, "KBD: C1",
    //  "Value: A8". Empty for a byte not known.
    std::wstring  GetContentsLine (Word address) const;

    //  The frame a cell's tip is drawn in: its range's label with the ROM
    //  behind it, and its color, 0 for the tip's. "RAM" for main RAM, and no
    //  frame in a bank with no regions.
    void   GetTipFrame     (Word address, std::wstring & label, uint32_t & argb) const;

    //  The tag at the end of a cell's tip's first line: "PC", "SP" or both,
    //  in the PC's or the stack's color; empty for neither.
    void   GetTipTag       (Word address, std::wstring & tag, uint32_t & argb) const;

    //  The outlines around an address's cell, outermost first: the PC's, the
    //  stack pointer's, then each breakpoint's on it.
    std::vector<uint32_t>  GetOutlineColors (Word address) const;

    //  A range the map shows on its own, under a header that gives its
    //  title and span.
    struct Band
    {
        std::wstring  title;
        Word          first     = 0;
        int           count     = 0;
        bool          isSection = false;   // a piece cut at GetSectionStarts, labeled by its region

        bool operator== (const Band & other) const = default;
    };

    //  Where the sections start: the I/O, each slot's I/O, each slot's ROM
    //  page, the expansion ROM, and the ROM or language card above it. The
    //  addresses are fixed, so banking changes a section's label and color
    //  and never the layout.
    static constexpr std::array<Word, 18>  kSectionStarts =
    {
        0xC000, 0xC080, 0xC090, 0xC0A0, 0xC0B0, 0xC0C0, 0xC0D0, 0xC0E0, 0xC0F0,
        0xC100, 0xC200, 0xC300, 0xC400, 0xC500, 0xC600, 0xC700, 0xC800, 0xD000,
    };

    //  Every section start for a machine, in order: kSectionStarts; below
    //  them, where each built-in range in RAM begins and ends, so zero page,
    //  the stack and the text and hi-res pages are sections, and so is the
    //  RAM between them; and above them, each page where the machine's
    //  system ROM changes part, as $F800 starts the Monitor.
    static std::vector<Word>  GetSectionStarts (const std::string & machineId = {});

    //  A section of RAM's label: the built-in range at an address, "Zero
    //  page", "Hi-res page 1", or "RAM" for none.
    static std::wstring       GetRamLabel     (Word address);

    //  The ranges cut at GetSectionStarts, each piece a section. A range no
    //  start cuts is left whole, and a section only where `isAllMemory`.
    static std::vector<Band>  SplitAtSections (const std::vector<Band> & ranges, bool isAllMemory, const std::string & machineId = {});

    //  Whether the map is cut into sections now, and the ranges on the map.
    bool                       IsSplit  () const { return m_isSplit; }
    const std::vector<Band> &  GetBands () const { return m_bands; }

    //  The height of a range's header, the room after a section's rows, and
    //  the note in place of an empty set.
    static constexpr int              kHeaderDip       = 20;
    static constexpr int              kSectionAfterDip = 10;

    //  A section's label's size, its header's height, and the room between
    //  its box and its cells on every side, which the label's line runs
    //  through at the top.
    static constexpr float            kSectionLabelDip  = 9.0f;
    static constexpr int              kSectionHeaderDip = 16;
    static constexpr int              kSectionPadDip    = 7;
    static constexpr const wchar_t  * kpszNoRangesNote = L"No ranges in this set. Include some with Edit ranges.";

    //  The ranges the map is focused on, which it then fits to the pane; or
    //  none of that, for all of memory.
    void   SetRanges   (std::vector<Band> ranges);
    void   ClearRanges ();
    bool   HasRanges   () const { return m_hasRanges; }

    //  Where a range's header is drawn within the pane, as scrolled.
    RECT   GetHeaderRect (size_t range) const;

    //  The options changed by a press on the pane; a cell clicked.
    void   SetOnOptionsChanged (std::function<void()> fn)     { m_onOptionsChanged = std::move (fn); }
    void   SetOnPickAddress    (std::function<void(Word)> fn) { m_onPickAddress    = std::move (fn); }

    //  A click with Ctrl on a cell; the cell the mouse frames changed.
    void   SetOnPickAccess     (std::function<void(Word, PickAction)> fn)    { m_onPickAccess    = std::move (fn); }
    void   SetOnHoverChanged   (std::function<void(std::optional<Word>)> fn) { m_onHoverChanged  = std::move (fn); }

    //  The bank the levels are of, and whether the machine has aux RAM, for
    //  the tip's location; the last writer and reader the machine looked up
    //  for a cell.
    void   SetShownBank        (Bank bank, bool hasAux) { m_shownBank = bank; m_hasAux = hasAux; }
    Bank   GetShownBank        () const                 { return m_shownBank; }
    void   SetHoverAccess      (std::shared_ptr<const HeatAccessHover> hover) { m_hoverAccess = std::move (hover); }

    //  The zoom, as a cell's height in pixels and the width a row's cells are
    //  stretched from; the addresses in a row and the rows; and how far the
    //  map is scrolled down in pixels from its top.
    int    GetCellPx    () const { return m_cellPx; }
    int    GetColumns   () const { return m_columns; }
    int    GetRows      () const;
    long   GetScrollTop () const { return m_scrollTop; }
    void   ZoomAt       (POINT point, float notches);
    void   ZoomIn       ();
    void   ZoomOut      ();
    void   ZoomTo       (int cellPx);
    void   ScrollBy     (int dy);
    void   ResetZoom    ();

    //  The cell size the map starts at, which the zoom's percentage is of;
    //  the zoom widget in the map's bottom-right corner; and its tip, for a
    //  point over its button.
    int                        GetStartCellPx  () const { return (m_fitCellPx > 0) ? m_fitCellPx : std::max (1, m_scaler.ToPx (kDefaultCellDip)); }
    const HeatMapZoomWidget &  GetZoomWidget   () const { return m_zoomWidget; }
    bool                       TryGetZoomTipAt (POINT point, RECT & anchor, std::wstring & text) const;

    //  The addresses a row holds for a width at a pitch: the largest power of
    //  two from kMinColumns to kMaxColumns whose cells fit, or twice that when
    //  those would stretch past kMaxStretch and each cell is at least a pixel
    //  wide; one address when even one cell at the pitch is wider than the
    //  width.
    static int  GetColumnsFor (long widthPx, long pitchPx);

    //  The addresses a row of a range holds, out of the map's columns: no
    //  more than the next power of two up from the range's size.
    static int  GetColumnsOf  (int columns, int count);

    //  Whether the map is taller than its area, so it scrolls and has a
    //  scrollbar; and where the scrollbar is drawn, empty without one.
    bool   HasVerticalScroll  () const { return m_hasVertBar; }
    RECT   GetVerticalBarRect () const;

    //  The map as drawn, a pixel each, the size of the map's area, drawn again
    //  first if anything changed it since; and the color one address is drawn in.
    const std::vector<uint32_t> &  GetPixels    ();
    uint32_t                       GetCellColor (Word address) const;

    static uint32_t      GetColor     (Mode mode, Byte execute, Byte read, Byte write, const Palette & palette);

    //  The same with Blend mixing the kinds that touched it instead of
    //  showing the hottest.
    static uint32_t      GetColor     (Mode mode, Byte execute, Byte read, Byte write, const Palette & palette, bool isBlend);

    //  An address's color with the reads before written and the changes
    //  drawn as the mode shows them, from its color without them.
    static uint32_t      GetChannelColor (Mode mode, Byte unwritten, Byte changed, uint32_t color, const Palette & palette);
    static std::wstring  GetModeLabel (Mode mode);

    //  The map's area within the pane: the part of the rows in view, without
    //  the scrollbar; and an address's cell within the pane, as scrolled.
    RECT                 GetMapRect  () const { return m_map; }

    //  The row of views, where the window puts the strip that picks the
    //  view, Blend, the set of ranges and the bank shown; empty while it has
    //  no room.
    RECT                 GetViewRowFreeRect () const;
    RECT                 GetCellRect (Word address) const;

    //  The address under a point in the map, a street counting as the cell
    //  before it; the address the mouse picks there, which may be a busier one
    //  near it; the mode under a point in the bar.
    std::optional<Word>  GetAddressAt (POINT point) const;
    std::optional<Word>  GetPickAt    (POINT point) const;

    std::optional<Word>  GetHover     () const;
    bool                 IsPressed    () const;

    //  Whether a point is over the map or its scrollbar.
    bool                 IsOverMap    (POINT point) const;

    //  The tip for a point on the map: the cell it picks, framed, and its
    //  address and what touched it, "$C65E  executed 120/s, read 3/s"; false
    //  off the map and while a button is down. The tip's first line is the
    //  meaning of the cell's color (GetTipLead) and `swatches` has the cell's
    //  color for that line, zero for the rest. `gradient` and `mark` are the
    //  cell's scale and its level on it (TryGetCellScale), or empty and -1.
    bool                 TryGetTipAt  (POINT point, RECT & anchor, std::wstring & text, std::vector<uint32_t> & swatches) const;
    bool                 TryGetTipAt  (POINT point, RECT & anchor, std::wstring & text, std::vector<uint32_t> & swatches,
                                       std::vector<uint32_t> & gradient, float & mark) const;

    //  What drew a cell: nothing touched it; or run as code, as an operand
    //  only, read, written, run as code and written, read before written, a
    //  write that changed its byte, or a Blend mix of kinds, which has no
    //  single color.
    enum class CellKind
    {
        Untouched,
        Code,
        Operand,
        Read,
        Written,
        SelfModifying,
        Unwritten,
        Changed,
        Blended,
    };

    static CellKind  GetCellKind (Mode mode, Byte execute, Byte read, Byte write, Byte unwritten, Byte changed, bool isOperand, bool isBlend);
    CellKind         GetCellKind (Word address) const;

    //  The first line of a cell's tip: the label of what drew the cell, and
    //  the color the cell is drawn in, for the swatch beside the label.
    struct TipLead
    {
        std::wstring  meaning;
        uint32_t      argb = 0;
    };

    TipLead              GetTipLead   (Word address) const;

    //  The scale a cell's color is from, faintest to full, as the legend's
    //  Ramp swatch draws it, and where on it the cell's level lies, 0 to 1;
    //  false for an untouched cell and a Blend mix, which have no one scale.
    bool                 TryGetCellScale (Word address, std::vector<uint32_t> & gradient, float & mark) const;
    std::vector<uint32_t>  GetScaleStops (CellKind kind) const;

    //  Whether the tip for a point is whole: the last writer and reader of
    //  the cell it picks are in, or nothing looks them up. A tip shown before
    //  them gains two lines when they arrive.
    bool                 IsTipComplete (POINT point) const;
    std::wstring         GetTipText   (Word address) const;

    //  The widening of a scrollbar under the mouse, carried out over frames.
    bool                 TickScrollbars (int64_t nowMs);

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent & ev) override;
    void                OnFocusChanged    (bool focused) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Heat map"; }

private:
    //  Where a range lies in the map, in pixels from the map's top: its
    //  header, then its rows, each holding `columns` addresses.
    struct Placed
    {
        int   columns   = 1;
        long  headerTop = 0;
        long  rowsTop   = 0;
        long  rows      = 0;
    };

    //  A cell: the range it is in, and how far into the range.
    struct Place
    {
        size_t  band   = 0;
        long    offset = 0;
    };

    //  An instruction drawn as one cell across its bytes: its opcode's place
    //  and address, and its length.
    struct Merged
    {
        Place  first;
        Word   opcode = 0;
        int    length = 0;
    };

    Byte                   GetShownLevel       (Word address) const;
    int                    GetPitch            () const { return m_cellPx + kStreetPx; }
    long                   GetColumnLeft       (const Placed & placed, long column) const;
    long                   GetColumnAt         (const Placed & placed, long x) const;
    long                   GetBandAt           (long y) const;
    std::optional<Place>   GetPlaceAt          (POINT point) const;
    std::optional<Place>   GetPickPlaceAt      (POINT point) const;
    std::optional<Place>   GetPlaceOf          (Word address) const;
    RECT                   GetPlaceRect        (const Place & place) const;
    Word                   GetAddressOf        (const Place & place) const;
    void                   PlaceBands          ();
    void                   FitRanges           ();
    int                    ComputeFitCellPx    ();
    void                   RescaleForDpi       (const RECT & boundsPx, const DxuiDpiScaler & scaler);
    std::wstring           FormatAmount        (Byte level) const;
    void                   ApplyCellPx         (int cellPx, POINT point);
    void                   PlaceMap            ();
    void                   ClampScroll         ();
    void                   LimitScroll         (long lowest, long highest);
    long                   GetMaxScroll        () const;
    void                   SyncScrollbars      ();
    void                   BuildCells          ();
    void                   BuildFrame          ();
    bool                   OnPress             (const DxuiMouseEvent & ev);
    bool                   OnRelease           (const DxuiMouseEvent & ev);
    bool                   OnDragOrHover       (const DxuiMouseEvent & ev);
    bool                   OnWheel             (const DxuiMouseEvent & ev);
    bool                   OnZoomWidget        (const DxuiMouseEvent & ev);
    RECT                   GetZoomCorner       () const;
    void                   PlaceZoomWidget     ();
    void                   SetHover            (const std::optional<Place> & hover);
    std::optional<Place>   GetKeyTarget        (const DxuiKeyEvent & ev) const;
    void                   ScrollToPlace       (const Place & place);
    void                   MeasureGutter       (IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void                   PaintBar            (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void                   PaintRowLabels      (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void                   PaintHeaders        (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void                   PaintHover          (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void                   PaintOverlays       (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void                   PaintDetail         (IDxuiTextRenderer & text, const IDxuiTheme & theme, const Place & place, Word address) const;
    bool                   IsMergeZoom         () const;
    int                    GetMergeLength      (const Place & place, Word address) const;
    std::optional<Merged>  GetMergedAt         (const Place & place) const;
    std::vector<RECT>      GetMergedRects      (const Merged & merged) const;
    void                   PaintInstruction    (IDxuiTextRenderer & text, const IDxuiTheme & theme, const Merged & merged) const;
    void                   PaintOutlines       (IDxuiTextRenderer & text, const std::vector<RECT> & rects, Word address) const;
    void                   PaintRegions        (IDxuiTextRenderer & text) const;
    void                   PaintSections       (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void                   SyncBands           ();
    bool                   CanSplit            () const;
    bool                   HasHeader           (size_t band) const { return m_hasRanges || (band < m_bands.size() && m_bands[band].isSection); }
    void                   MarkSymbols         ();
    void                   ForEachVisible      (const std::function<void (const Place &, Word)> & visit) const;
    std::wstring           DescribeMarks       (Word address) const;
    std::wstring           DescribeInstruction (Word address) const;

    std::vector<Byte>          m_execute;
    std::vector<Byte>          m_read;
    std::vector<Byte>          m_write;
    std::vector<Byte>          m_unwritten;
    std::vector<Byte>          m_edited;
    std::vector<Byte>          m_changed;
    std::vector<uint32_t>      m_cells;
    std::vector<uint32_t>      m_frame;
    Palette                    m_palette;
    HeatMapOptions             m_options;
    DxuiDpiScaler              m_scaler;
    DxuiScrollbar              m_vertBar;
    std::optional<Place>       m_hover;
    std::vector<Band>          m_bands        = { Band { {}, 0, kAddressCount } };
    std::vector<Placed>        m_placed;
    std::vector<Band>          m_ranges       = { Band { {}, 0, kAddressCount } };   // the set's ranges, or all of memory, before sections cut them
    bool                       m_isSplit      = false;
    long                       m_contentH     = 0;
    bool                       m_hasRanges    = false;
    bool                       m_isFitted     = true;
    RECT                       m_fittedFor    = {};     // the bounds the map was last fitted to
    int                        m_fitCellPx    = 0;      // the fitted cell size, the zoom's 100%
    double                     m_top          = 0.0;
    int                        m_cellPx       = 0;
    int                        m_columns      = kMaxColumns / 4;
    long                       m_rowPx        = 0;
    int                        m_gutterPx     = 0;
    long                       m_labelLinePx  = 0;       // a row label's line, measured with the gutter
    RECT                       m_map          = {};
    long                       m_areaBottom   = 0;       // the bottom of the map's area, which a map scrolled past an end fills
    bool                       m_hasVertBar   = false;
    long                       m_scrollTop    = 0;
    std::optional<POINT>       m_press;
    long                       m_pressTop     = 0;
    bool                       m_isDragging   = false;
    bool                       m_isFrameStale = false;
    bool                       m_isRebuilding = false;
    RECT                       m_noteRect     = {};
    std::function<void()>      m_onOptionsChanged;
    std::function<void(Word)>  m_onPickAddress;

    std::function<void(Word, PickAction)>     m_onPickAccess;
    std::function<void(std::optional<Word>)>  m_onHoverChanged;
    std::shared_ptr<const HeatAccessHover>    m_hoverAccess;
    Bank                                      m_shownBank = Bank::Cpu;
    bool                                      m_hasAux    = false;

    std::vector<Byte>                                m_opcodes;
    std::vector<int16_t>                             m_values;
    std::shared_ptr<const std::vector<std::string>>  m_opcodeForms;
    std::optional<Word>                              m_pc;
    std::optional<Word>                              m_stack;
    std::vector<Breakpoint>                          m_breakpoints;
    std::shared_ptr<const HeatMapSymbols>            m_symbols;
    Bank                                             m_symbolsBank   = Bank::Cpu;
    std::vector<Byte>                                m_symbolMarks;
    std::vector<uint16_t>                            m_regions;
    MemoryRegionKeys::SlotCards                      m_slotCards;
    std::string                                      m_machineId;
    MemoryRegionKeys::Colors                         m_regionColors;
    HeatMapZoomWidget                                m_zoomWidget;
    RECT                                             m_zoomPlacedFor = {};   // the bounds the zoom widget was placed in
};
