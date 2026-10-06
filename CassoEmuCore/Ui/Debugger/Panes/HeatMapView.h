#pragma once

#include "Pch.h"

#include "Debugger/HeatMapOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView
//
//  The 64 KB address space as a grid, one cell an address and one row a
//  page, each address drawn in the color of what touched it, brighter the
//  more it was touched. Code is drawn in one color and data in two, reads and
//  writes, so the program's code stands apart from what it works on. Every
//  cell is drawn apart from its neighbors by a street of the page between
//  them, and an untouched one is a gray, not the page.
//
//  Over the map two rows. The first holds the views: All shows everything,
//  with code ahead of data where an address is both; Code shows executes
//  alone; Data shows reads and writes alone. Beside them the colors' key, and
//  the address under the mouse and what touched it. The second holds Fading
//  and Cumulative, how long the heat takes to fade or a Reset counts for the
//  totals, and Reset zoom. Down the left, page numbers.
//
//  The wheel zooms about the mouse and a drag pans; a click on a cell shows
//  its address in a memory window. While cells are small the readout takes
//  the busiest cell near the mouse, so a lone hot byte is easy to land on.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapView : public IDxuiControl
{
public:
    using Mode = HeatMapOptions::View;

    struct Palette
    {
        uint32_t  background = 0;
        uint32_t  cold       = 0;
        uint32_t  execute    = 0;
        uint32_t  read       = 0;
        uint32_t  write      = 0;

        bool operator== (const Palette & other) const = default;
    };

    //  What a press on the second row does.
    enum class Action
    {
        Fading,
        Cumulative,
        Fade,
        ResetCounts,
        ResetZoom,
    };

    static constexpr int    kSide           = 256;     // pages, and bytes a page
    static constexpr int    kBarDip         = 22;      // each of the two rows over the map
    static constexpr int    kBarRows        = 2;
    static constexpr int    kTabDip         = 52;
    static constexpr int    kWideTabDip     = 88;
    static constexpr int    kGutterDip      = 34;
    static constexpr int    kInsetDip       = 8;       // right of the map and the readout, and below the map
    static constexpr int    kModeCount      = 3;

    //  A cell's side, without the street of one pixel after it: three at 96
    //  dpi to start, from one to kMaxCellPx as the wheel zooms.
    static constexpr int    kStreetPx       = 1;
    static constexpr int    kDefaultCellDip = 3;
    static constexpr int    kMaxCellPx      = 64;
    static constexpr float  kZoomPerNotch   = 1.25f;

    //  Below this side a cell is hard to land on, and the readout takes the
    //  busiest cell within kSnapDip of the mouse instead.
    static constexpr int    kComfortCellDip = 8;
    static constexpr int    kSnapDip        = 4;

    //  How far the mouse moves with the button down before it pans.
    static constexpr int    kDragDip        = 4;

    //  How far from the cold gray toward its color the faintest touched
    //  address is drawn, so one touch is still visible.
    static constexpr float  kFaintest       = 0.3f;

    void   SetLevels  (const std::vector<Byte> & execute, const std::vector<Byte> & read, const std::vector<Byte> & write);
    void   SetPalette (const Palette & palette);
    void   SetMode    (Mode mode);
    Mode   GetMode    () const { return m_options.view; }

    void                    SetOptions (const HeatMapOptions & options);
    const HeatMapOptions &  GetOptions () const { return m_options; }

    //  The options changed by a press on the pane; Reset counts pressed; a
    //  cell clicked.
    void   SetOnOptionsChanged (std::function<void()> fn)     { m_onOptionsChanged = std::move (fn); }
    void   SetOnResetCounts    (std::function<void()> fn)     { m_onResetCounts    = std::move (fn); }
    void   SetOnPickAddress    (std::function<void(Word)> fn) { m_onPickAddress    = std::move (fn); }

    //  The zoom, as a cell's side in pixels, and how far the map is scrolled
    //  in pixels from its top left.
    int    GetCellPx  () const { return m_cellPx; }
    POINT  GetScroll  () const { return m_scroll; }
    void   ZoomAt     (POINT point, float notches);
    void   ScrollBy   (int dx, int dy);
    void   ResetZoom  ();

    //  The map as drawn, a pixel each, the size of the map's area; and the
    //  color one address is drawn in.
    const std::vector<uint32_t> &  GetPixels    () const { return m_frame; }
    uint32_t                       GetCellColor (Word address) const;

    static uint32_t      GetColor       (Mode mode, Byte execute, Byte read, Byte write, const Palette & palette);
    static std::wstring  GetModeLabel   (Mode mode);
    std::wstring         GetActionLabel (Action action) const;

    //  The map's area within the pane.
    RECT                 GetMapRect () const;

    //  The address under a point in the map, a street counting as the cell
    //  before it; the address the readout takes for a point, which may be a
    //  busier one near it; the mode and the action under a point in the bar.
    std::optional<Word>    GetAddressAt (POINT point) const;
    std::optional<Word>    GetPickAt    (POINT point) const;
    std::optional<Mode>    GetModeAt    (POINT point) const;
    std::optional<Action>  GetActionAt  (POINT point) const;
    std::optional<Word>    GetHover     () const { return m_hover; }
    bool                   IsPressed    () const { return m_press.has_value(); }

    //  "$C65E  executed, read": the address the mouse picks and what touched
    //  it, or the address alone when nothing did.
    std::wstring         GetReadout () const;

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Heat map"; }

private:
    //  The swatches the bar shows: each kind's label and color.
    using KeyList = std::vector<std::pair<std::wstring, uint32_t>>;

    struct Button
    {
        Action  action = Action::Fading;
        float   left   = 0.0f;
        float   width  = 0.0f;
    };

    std::vector<Button>  GetButtons    () const;
    Byte                 GetShownLevel (Word address) const;
    int                  GetPitch      () const { return m_cellPx + kStreetPx; }
    void                 ClampScroll   ();
    void                 BuildCells    ();
    void                 BuildFrame    ();
    void                 RunAction     (Action action);
    bool                 OnPress       (const DxuiMouseEvent & ev);
    bool                 OnRelease     (const DxuiMouseEvent & ev);
    bool                 OnDragOrHover (const DxuiMouseEvent & ev);
    void                 PaintBar      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void                 PaintActions  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void                 PaintPages    (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void                 PaintHover    (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;

    std::vector<Byte>          m_execute;
    std::vector<Byte>          m_read;
    std::vector<Byte>          m_write;
    std::vector<uint32_t>      m_cells;
    std::vector<uint32_t>      m_frame;
    Palette                    m_palette;
    HeatMapOptions             m_options;
    DxuiDpiScaler              m_scaler;
    std::optional<Word>        m_hover;
    int                        m_cellPx       = 0;
    POINT                      m_scroll       = {};
    std::optional<POINT>       m_press;
    POINT                      m_pressScroll  = {};
    bool                       m_isDragging   = false;
    bool                       m_isFrameStale = false;
    std::function<void()>      m_onOptionsChanged;
    std::function<void()>      m_onResetCounts;
    std::function<void(Word)>  m_onPickAddress;
};
