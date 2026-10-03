#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView
//
//  The 64 KB address space as a grid, one cell an address and one row a
//  page, each address drawn in the color of what touched it lately, brighter
//  the more it was touched. Code is drawn in one color and data in two, reads
//  and writes, so the program's code stands apart from what it works on.
//
//  Over the map a row of modes: All shows everything, with code ahead of
//  data where an address is both; Code shows executes alone; Data shows reads
//  and writes alone. Beside them, the address under the mouse and what
//  touched it. Down the left, every 32nd page's number.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapView : public IDxuiControl
{
public:
    enum class Mode
    {
        All,
        Code,
        Data,
    };

    struct Palette
    {
        uint32_t  background = 0;
        uint32_t  execute    = 0;
        uint32_t  read       = 0;
        uint32_t  write      = 0;

        bool operator== (const Palette & other) const = default;
    };

    static constexpr int    kSide        = 256;     // pages, and bytes a page
    static constexpr int    kBarDip      = 22;
    static constexpr int    kTabDip      = 52;
    static constexpr int    kGutterDip   = 34;
    static constexpr int    kInsetDip    = 8;       // right of the map and the readout, and below the map
    static constexpr int    kLabelPages  = 0x20;
    static constexpr int    kModeCount   = 3;

    //  How far from the background toward its color the faintest touched
    //  address is drawn, so one touch is still visible.
    static constexpr float  kFaintest    = 0.3f;

    void   SetLevels  (const std::vector<Byte> & execute, const std::vector<Byte> & read, const std::vector<Byte> & write);
    void   SetPalette (const Palette & palette);
    void   SetMode    (Mode mode);
    Mode   GetMode    () const { return m_mode; }

    const std::vector<uint32_t> &  GetPixels () const { return m_pixels; }

    static uint32_t      GetColor     (Mode mode, Byte execute, Byte read, Byte write, const Palette & palette);
    static std::wstring  GetModeLabel (Mode mode);

    //  The address under a point in the map, and the mode under a point in
    //  the bar; nothing elsewhere.
    std::optional<Word>  GetAddressAt (POINT point) const;
    std::optional<Mode>  GetModeAt    (POINT point) const;

    //  "$C65E  executed, read": the address under the mouse and what touched
    //  it, or the address alone when nothing did lately.
    std::wstring         GetReadout   () const;

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Heat map"; }

private:
    //  The swatches the bar shows: each kind's label and color.
    using KeyList = std::vector<std::pair<std::wstring, uint32_t>>;

    void   BuildPixels ();
    RECT   GetMapRect  () const;
    void   PaintBar    (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void   PaintPages  (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;

    std::vector<Byte>      m_execute;
    std::vector<Byte>      m_read;
    std::vector<Byte>      m_write;
    std::vector<uint32_t>  m_pixels;
    Palette                m_palette;
    Mode                   m_mode     = Mode::All;
    DxuiDpiScaler          m_scaler;
    std::optional<Word>    m_hover;
};
