#pragma once

#include "Pch.h"

#include "Debugger/HeatAccessJump.h"
#include "Debugger/HeatMapOptions.h"

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
//  the others; a row is always as tall as the zoom's cell. Only kMinColumns
//  wider than the pane at the zoom's size scroll across.
//
//  Over the map a row of views: All shows everything, with code ahead of data
//  where an address is both; Code shows executes alone; Data shows reads and
//  writes alone; Changed shows the writes that changed their byte alone, in
//  a color of their own. In All and Data a read of RAM nothing had written
//  since power-on shows in its own color over the rest, as hot as such
//  reads were. Beside them the colors' key.
//
//  The wheel scrolls up and down, Shift with it across, and Ctrl with it
//  zooms about the mouse; a drag pans, and the scrollbars show and move the
//  part in view. A click on a cell shows its address in a memory window.
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
//  enabled breakpoint, and an address with a symbol is tinted; a byte only
//  ever run as an instruction's operand is drawn dimmer than its opcode, so
//  where each instruction starts shows. Zoomed in far enough, a cell shows
//  its value, and further in, its opcode's form or its bits.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapView : public IDxuiControl
{
public:
    using Mode = HeatMapOptions::View;
    using Bank = HeatMapOptions::Bank;

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

    //  How far an address with a symbol is tinted toward the symbol color,
    //  and an operand byte's code color toward the cold gray.
    static constexpr float  kSymbolTint  = 0.25f;
    static constexpr float  kOperandDim  = 0.55f;

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
    static uint32_t      GetDetailTextColor (uint32_t cell, uint32_t foreground, uint32_t background);

    static constexpr int    kAddressCount   = 0x10000;
    static constexpr int    kMinColumns     = 16;
    static constexpr int    kMaxColumns     = 1024;
    static constexpr int    kBarDip         = 22;      // the row of views over the map
    static constexpr int    kTabDip         = 52;
    static constexpr int    kGutterDip      = 44;      // the row labels' column, until a paint measures them
    static constexpr int    kInsetDip       = 8;       // right of the map and below it
    static constexpr int    kScrollbarDip   = 10;
    static constexpr int    kModeCount      = 4;

    //  A cell's side, without the street of one pixel after it: three at 96
    //  dpi to start, from one to kMaxCellPx as the zoom goes.
    static constexpr int    kStreetPx       = 1;
    static constexpr int    kDefaultCellDip = 3;
    static constexpr int    kMaxCellPx      = 64;
    static constexpr float  kZoomPerNotch   = 1.25f;

    //  The most a row's cells are widened to fill the pane before the row
    //  takes twice the addresses instead, each a little narrower than the
    //  zoom's: a column stays between three quarters and one and a half times
    //  the zoom's width, except where kMaxColumns or a one-pixel cell stop it.
    static constexpr double kMaxStretch     = 1.5;

    //  How far one notch of the wheel scrolls, rounded to whole rows.
    static constexpr int    kWheelStepDip   = 48;

    //  Below this side a cell is hard to land on, and the pick is the busiest
    //  cell within kSnapDip of the mouse instead.
    static constexpr int    kComfortCellDip = 8;
    static constexpr int    kSnapDip        = 4;

    //  How far the mouse moves with the button down before it pans.
    static constexpr int    kDragDip        = 4;

    //  How far from the cold gray toward its color the faintest touched
    //  address is drawn, so one touch is still visible.
    static constexpr float  kFaintest       = 0.3f;

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
    //  so in place of the color key.
    static constexpr const wchar_t * kpszRebuildingNote = L"Rebuilding";

    //  The key's swatch for Blend's color for code that is also written.
    static constexpr const wchar_t * kpszSelfModifyingKey = L"Self-modifying";

    void   SetRebuilding (bool isRebuilding) { m_isRebuilding = isRebuilding; }
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

    //  The outlines around an address's cell, outermost first: the PC's, the
    //  stack pointer's, then each breakpoint's on it.
    std::vector<uint32_t>  GetOutlineColors (Word address) const;

    //  A range the map shows on its own, under a header that gives its
    //  title and span.
    struct Band
    {
        std::wstring  title;
        Word          first = 0;
        int           count = 0;

        bool operator== (const Band & other) const = default;
    };

    //  The height of a range's header, and the note an empty set shows.
    static constexpr int              kHeaderDip       = 20;
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
    //  map is scrolled in pixels from its top left.
    int    GetCellPx  () const { return m_cellPx; }
    int    GetColumns () const { return m_columns; }
    int    GetRows    () const;
    POINT  GetScroll  () const { return m_scroll; }
    void   ZoomAt     (POINT point, float notches);
    void   ZoomIn     ();
    void   ZoomOut    ();
    void   ScrollBy   (int dx, int dy);
    void   ResetZoom  ();

    //  The addresses a row holds for a width at a pitch: the largest power of
    //  two from kMinColumns to kMaxColumns whose cells fit, or twice that when
    //  those would stretch past kMaxStretch and a cell still has a pixel.
    static int  GetColumnsFor (long widthPx, long pitchPx);

    //  The addresses a row of a range holds, out of the map's columns: no
    //  more than the next power of two up from the range's size.
    static int  GetColumnsOf  (int columns, int count);

    //  Whether the map is wider or taller than its area, so it scrolls that
    //  way and shows a scrollbar for it.
    bool   HasHorizontalScroll () const { return m_hasHorzBar; }
    bool   HasVerticalScroll   () const { return m_hasVertBar; }

    //  Where each scrollbar is drawn; empty when it is not.
    RECT   GetHorizontalBarRect () const;
    RECT   GetVerticalBarRect   () const;

    //  The map as drawn, a pixel each, the size of the map's area, drawn again
    //  first if anything changed it since; and the color one address is drawn in.
    const std::vector<uint32_t> &  GetPixels    ();
    uint32_t                       GetCellColor (Word address) const;

    static uint32_t      GetColor     (Mode mode, Byte execute, Byte read, Byte write, const Palette & palette);

    //  The same with an operand byte's code dimmed, and with Blend mixing
    //  the kinds that touched it instead of showing the hottest.
    static uint32_t      GetColor     (Mode mode, Byte execute, Byte read, Byte write, const Palette & palette, bool isOperand, bool isBlend);

    //  An address's color with the reads before written and the changes
    //  drawn as the mode shows them, from its color without them.
    static uint32_t      GetChannelColor (Mode mode, Byte unwritten, Byte changed, uint32_t color, const Palette & palette);
    static std::wstring  GetModeLabel (Mode mode);

    //  The map's area within the pane: the part of the rows in view, without
    //  the scrollbars; and an address's cell within the pane, as scrolled.
    RECT                 GetMapRect  () const { return m_map; }
    RECT                 GetCellRect (Word address) const;

    //  The address under a point in the map, a street counting as the cell
    //  before it; the address the mouse picks there, which may be a busier one
    //  near it; the mode under a point in the bar.
    std::optional<Word>  GetAddressAt (POINT point) const;
    std::optional<Word>  GetPickAt    (POINT point) const;
    std::optional<Mode>  GetModeAt    (POINT point) const;
    std::optional<Word>  GetHover     () const;
    bool                 IsPressed    () const;

    //  Whether a point is over the map or its scrollbars.
    bool                 IsOverMap    (POINT point) const;

    //  The tip for a point on the map: the cell it picks, framed, and its
    //  address and what touched it, "$C65E  executed 120/s, read 3/s"; false
    //  off the map and while a button is down.
    bool                 TryGetTipAt  (POINT point, RECT & anchor, std::wstring & text) const;
    std::wstring         GetTipText   (Word address) const;

    //  The widening of a scrollbar under the mouse, carried out over frames.
    bool                 TickScrollbars (int64_t nowMs);

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Heat map"; }

private:
    //  The swatches the bar shows: each kind's label and color.
    using KeyList = std::vector<std::pair<std::wstring, uint32_t>>;

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

    Byte                 GetShownLevel  (Word address) const;
    int                  GetPitch       () const { return m_cellPx + kStreetPx; }
    long                 GetColumnLeft  (const Placed & placed, long column) const;
    long                 GetColumnAt    (const Placed & placed, long x) const;
    long                 GetBandAt      (long y) const;
    std::optional<Place> GetPlaceAt     (POINT point) const;
    std::optional<Place> GetPickPlaceAt (POINT point) const;
    std::optional<Place> GetPlaceOf     (Word address) const;
    RECT                 GetPlaceRect   (const Place & place) const;
    Word                 GetAddressOf   (const Place & place) const;
    void                 PlaceBands     ();
    void                 FitRanges      ();
    std::wstring         FormatAmount   (Byte level) const;
    void                 ApplyCellPx    (int cellPx, POINT point);
    void                 PlaceMap       ();
    void                 ClampScroll    ();
    void                 SyncScrollbars ();
    void                 BuildCells     ();
    void                 BuildFrame     ();
    bool                 OnPress        (const DxuiMouseEvent & ev);
    bool                 OnRelease      (const DxuiMouseEvent & ev);
    bool                 OnDragOrHover  (const DxuiMouseEvent & ev);
    bool                 OnWheel        (const DxuiMouseEvent & ev);
    void                 SetHover       (const std::optional<Place> & hover);
    void                 MeasureGutter  (IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void                 PaintBar       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void                 AddChannelKeys (KeyList & keys) const;
    void                 PaintRowLabels (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void                 PaintHeaders   (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void                 PaintHover     (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void                 PaintOverlays  (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void                 PaintDetail    (IDxuiTextRenderer & text, const IDxuiTheme & theme, const Place & place, Word address) const;
    void                 PaintOutlines  (IDxuiTextRenderer & text, const Place & place, Word address) const;
    void                 MarkSymbols    ();
    void                 ForEachVisible (const std::function<void (const Place &, Word)> & visit) const;
    std::wstring         DescribeMarks  (Word address) const;

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
    DxuiScrollbar              m_horzBar;
    DxuiScrollbar              m_vertBar;
    std::optional<Place>       m_hover;
    std::vector<Band>          m_bands        = { Band { {}, 0, kAddressCount } };
    std::vector<Placed>        m_placed;
    long                       m_contentH     = 0;
    bool                       m_hasRanges    = false;
    bool                       m_isFitted     = false;
    double                     m_top          = 0.0;
    int                        m_cellPx       = 0;
    int                        m_columns      = kMaxColumns / 4;
    long                       m_rowPx        = 0;
    int                        m_gutterPx     = 0;
    RECT                       m_map          = {};
    bool                       m_hasHorzBar   = false;
    bool                       m_hasVertBar   = false;
    POINT                      m_scroll       = {};
    std::optional<POINT>       m_press;
    POINT                      m_pressScroll  = {};
    bool                       m_isDragging   = false;
    bool                       m_isFrameStale = false;
    bool                       m_isRebuilding = false;
    std::function<void()>      m_onOptionsChanged;
    std::function<void(Word)>  m_onPickAddress;

    std::function<void(Word, PickAction)>     m_onPickAccess;
    std::function<void(std::optional<Word>)>  m_onHoverChanged;
    std::shared_ptr<const HeatAccessHover>    m_hoverAccess;
    Bank                                      m_shownBank = Bank::Cpu;
    bool                                      m_hasAux    = false;

    std::vector<Byte>                                 m_opcodes;
    std::vector<int16_t>                              m_values;
    std::shared_ptr<const std::vector<std::string>>   m_opcodeForms;
    std::optional<Word>                               m_pc;
    std::optional<Word>                               m_stack;
    std::vector<Breakpoint>                           m_breakpoints;
    std::shared_ptr<const HeatMapSymbols>             m_symbols;
    Bank                                              m_symbolsBank = Bank::Cpu;
    std::vector<Byte>                                 m_symbolMarks;
};
