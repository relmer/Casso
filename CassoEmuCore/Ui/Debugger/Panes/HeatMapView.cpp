#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetLevels
//
//  One level per address for each kind; empty vectors are a cold map.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetLevels (const std::vector<Byte> & execute, const std::vector<Byte> & read, const std::vector<Byte> & write)
{
    m_execute = execute;
    m_read    = read;
    m_write   = write;

    BuildCells();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetPalette
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetPalette (const Palette & palette)
{
    if (palette == m_palette)
    {
        return;
    }

    m_palette = palette;

    BuildCells();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetMode
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetMode (Mode mode)
{
    m_options.view = mode;

    BuildCells();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetOptions
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetOptions (const HeatMapOptions & options)
{
    m_options = options;

    BuildCells();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::ZoomAt
//
//  Each notch up makes a cell kZoomPerNotch times larger, and each down that
//  much smaller, by at least a pixel; the address under the point stays under
//  it. A point off the map zooms about the map's center.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ZoomAt (POINT point, float notches)
{
    RECT    map       = GetMapRect();
    double  pitch     = (double) GetPitch();
    int     cell      = m_cellPx;
    double  contentX  = 0.0;
    double  contentY  = 0.0;



    if (notches == 0.0f || map.right <= map.left || map.bottom <= map.top)
    {
        return;
    }

    if (point.x < map.left || point.x >= map.right || point.y < map.top || point.y >= map.bottom)
    {
        point = { (map.left + map.right) / 2, (map.top + map.bottom) / 2 };
    }

    contentX = (double) (point.x - map.left + m_scroll.x) / pitch;
    contentY = (double) (point.y - map.top  + m_scroll.y) / pitch;

    cell = (int) std::lround ((double) m_cellPx * std::pow ((double) kZoomPerNotch, (double) notches));

    if (cell == m_cellPx)
    {
        cell += (notches > 0.0f) ? 1 : -1;
    }

    m_cellPx = std::clamp (cell, 1, kMaxCellPx);
    pitch    = (double) GetPitch();

    m_scroll.x = (long) std::lround (contentX * pitch - (double) (point.x - map.left));
    m_scroll.y = (long) std::lround (contentY * pitch - (double) (point.y - map.top));

    ClampScroll();
    BuildFrame();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::ScrollBy
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ScrollBy (int dx, int dy)
{
    m_scroll.x += dx;
    m_scroll.y += dy;

    ClampScroll();
    BuildFrame();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::ResetZoom
//
//  Back to the starting cell size, with the map's top left in view.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ResetZoom()
{
    m_cellPx = std::max (1, m_scaler.ToPx (kDefaultCellDip));
    m_scroll = {};

    ClampScroll();
    BuildFrame();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::ClampScroll
//
//  The map scrolls no further than its far edge reaching the area's; a map
//  smaller than its area does not scroll at all.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::ClampScroll()
{
    RECT  map     = GetMapRect();
    long  content = (long) kSide * GetPitch();



    m_scroll.x = std::clamp (m_scroll.x, 0L, std::max (0L, content - (map.right  - map.left)));
    m_scroll.y = std::clamp (m_scroll.y, 0L, std::max (0L, content - (map.bottom - map.top)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetColor
//
//  An address takes the color of the kind that touched it most, as the mode
//  lets it be seen, mixed toward that color from the cold gray by how hot it
//  is. Where code and data are equally hot, code shows. A write shows over a
//  read as hot. An address nothing touched is the cold gray.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HeatMapView::GetColor (Mode mode, Byte execute, Byte read, Byte write, const Palette & palette)
{
    constexpr float  kTop  = 255.0f;
    Byte             data  = std::max (read, write);
    uint32_t         hue   = (write >= read) ? palette.write : palette.read;
    Byte             level = data;



    if (mode == Mode::Code || (mode == Mode::All && execute >= data))
    {
        hue   = palette.execute;
        level = execute;
    }

    if (level == 0)
    {
        return palette.cold | 0xFF000000u;
    }

    return DxuiColor::Mix (palette.cold | 0xFF000000u, hue | 0xFF000000u,
                           kFaintest + (1.0f - kFaintest) * (float) level / kTop) | 0xFF000000u;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetModeLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapView::GetModeLabel (Mode mode)
{
    switch (mode)
    {
    case Mode::Code: return L"Code";
    case Mode::Data: return L"Data";
    default:         return L"All";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetActionLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapView::GetActionLabel (Action action) const
{
    switch (action)
    {
    case Action::Fading:      return L"Fading";
    case Action::Cumulative:  return L"Cumulative";
    case Action::Fade:        return std::format (L"Fade {} s", m_options.fadeSeconds);
    case Action::ResetCounts: return L"Reset counts";
    default:                  return L"Reset zoom";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetShownLevel
//
//  How hot an address is as the mode shows it.
//
////////////////////////////////////////////////////////////////////////////////

Byte HeatMapView::GetShownLevel (Word address) const
{
    constexpr size_t  kCount  = (size_t) kSide * kSide;
    Byte              execute = (m_execute.size() == kCount) ? m_execute[address] : (Byte) 0;
    Byte              read    = (m_read.size()    == kCount) ? m_read[address]    : (Byte) 0;
    Byte              write   = (m_write.size()   == kCount) ? m_write[address]   : (Byte) 0;



    switch (m_options.view)
    {
    case Mode::Code: return execute;
    case Mode::Data: return std::max (read, write);
    default:         return std::max ({ execute, read, write });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::BuildCells
//
//  Each address's color; a kind with no levels is cold everywhere.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::BuildCells()
{
    constexpr size_t  kCount     = (size_t) kSide * kSide;
    bool              hasExecute = m_execute.size() == kCount;
    bool              hasRead    = m_read.size()    == kCount;
    bool              hasWrite   = m_write.size()   == kCount;



    m_cells.resize (kCount);

    for (size_t address = 0; address < kCount; address++)
    {
        m_cells[address] = GetColor (m_options.view,
                                     hasExecute ? m_execute[address] : (Byte) 0,
                                     hasRead    ? m_read[address]    : (Byte) 0,
                                     hasWrite   ? m_write[address]   : (Byte) 0,
                                     m_palette);
    }

    BuildFrame();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetCellColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HeatMapView::GetCellColor (Word address) const
{
    return m_cells.empty() ? (m_palette.cold | 0xFF000000u) : m_cells[address];
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::BuildFrame
//
//  The map's area a pixel at a time, as scrolled: each cell its color, the
//  street after it and anything past the map the page.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::BuildFrame()
{
    RECT              map        = GetMapRect();
    long              width      = map.right  - map.left;
    long              height     = map.bottom - map.top;
    long              pitch      = GetPitch();
    uint32_t          background = m_palette.background | 0xFF000000u;
    std::vector<int>  columns;
    std::vector<int>  rows;



    //  A pane behind another tab is drawn when it next paints, not on every
    //  snapshot the machine sends meanwhile.
    m_isFrameStale = !m_visible;

    if (m_isFrameStale)
    {
        return;
    }

    if (width <= 0 || height <= 0 || m_cellPx <= 0)
    {
        m_frame.clear();
        return;
    }

    //  The cell each pixel column and row falls in, or -1 for a street.
    columns.resize ((size_t) width);
    rows.resize    ((size_t) height);

    for (long x = 0; x < width; x++)
    {
        long  at = x + m_scroll.x;



        columns[(size_t) x] = (at / pitch < kSide && at % pitch < m_cellPx) ? (int) (at / pitch) : -1;
    }

    for (long y = 0; y < height; y++)
    {
        long  at = y + m_scroll.y;



        rows[(size_t) y] = (at / pitch < kSide && at % pitch < m_cellPx) ? (int) (at / pitch) : -1;
    }

    m_frame.assign ((size_t) (width * height), background);

    for (long y = 0; y < height; y++)
    {
        uint32_t  * line = m_frame.data() + (size_t) (y * width);
        int         row  = rows[(size_t) y];



        if (row < 0)
        {
            continue;
        }

        for (long x = 0; x < width; x++)
        {
            int  column = columns[(size_t) x];



            if (column >= 0)
            {
                line[x] = GetCellColor ((Word) ((row << 8) | column));
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetMapRect
//
//  Below the two rows and right of the page numbers, filling the rest of the
//  pane but a margin.
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapView::GetMapRect() const
{
    RECT  map = m_boundsDip;



    map.left  += m_scaler.ToPx (kGutterDip);
    map.top   += m_scaler.ToPx (kBarDip) * kBarRows;
    map.right  = std::max (map.left, map.right  - m_scaler.ToPx (kInsetDip));
    map.bottom = std::max (map.top,  map.bottom - m_scaler.ToPx (kInsetDip));
    return map;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetAddressAt
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> HeatMapView::GetAddressAt (POINT point) const
{
    RECT  map    = GetMapRect();
    long  pitch  = GetPitch();
    long  column = 0;
    long  row    = 0;



    if (m_cellPx <= 0 || point.x < map.left || point.x >= map.right || point.y < map.top || point.y >= map.bottom)
    {
        return std::nullopt;
    }

    column = (point.x - map.left + m_scroll.x) / pitch;
    row    = (point.y - map.top  + m_scroll.y) / pitch;

    if (column >= kSide || row >= kSide)
    {
        return std::nullopt;
    }

    return (Word) ((row << 8) | column);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetPickAt
//
//  While cells are smaller than kComfortCellDip, a point over a cold cell
//  takes the hottest cell within kSnapDip of it, the nearer of two as hot;
//  with none, or with cells large enough to land on, the cell under it.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> HeatMapView::GetPickAt (POINT point) const
{
    std::optional<Word>  under   = GetAddressAt (point);
    RECT                 map     = GetMapRect();
    long                 pitch   = GetPitch();
    long                 reach   = m_scaler.ToPx (kSnapDip);
    long                 atX     = point.x - map.left + m_scroll.x;
    long                 atY     = point.y - map.top  + m_scroll.y;
    Word                 best    = 0;
    Byte                 hottest = 0;
    double               nearest = 0.0;



    if (!under.has_value() || m_cellPx >= m_scaler.ToPx (kComfortCellDip) || GetShownLevel (*under) > 0)
    {
        return under;
    }

    best = *under;

    for (long row = std::max (0L, atY - reach) / pitch; row <= std::min ((long) kSide - 1, (atY + reach) / pitch); row++)
    {
        for (long column = std::max (0L, atX - reach) / pitch; column <= std::min ((long) kSide - 1, (atX + reach) / pitch); column++)
        {
            Word    address  = (Word) ((row << 8) | column);
            Byte    level    = GetShownLevel (address);
            double  dx       = (double) (column * pitch) + (double) m_cellPx / 2.0 - (double) atX;
            double  dy       = (double) (row    * pitch) + (double) m_cellPx / 2.0 - (double) atY;
            double  distance = dx * dx + dy * dy;



            if (level == 0 || level < hottest || (level == hottest && distance >= nearest))
            {
                continue;
            }

            best    = address;
            hottest = level;
            nearest = distance;
        }
    }

    return best;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetModeAt
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapView::Mode> HeatMapView::GetModeAt (POINT point) const
{
    long  tab   = m_scaler.ToPx (kTabDip);
    long  left  = m_boundsDip.left + m_scaler.ToPx (kGutterDip);
    long  index = 0;



    if (tab <= 0 || point.y < m_boundsDip.top || point.y >= m_boundsDip.top + m_scaler.ToPx (kBarDip) || point.x < left)
    {
        return std::nullopt;
    }

    index = (point.x - left) / tab;

    if (index >= kModeCount)
    {
        return std::nullopt;
    }

    return (Mode) index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetButtons
//
//  The second row: Fading and Cumulative, then the fade time while fading or
//  Reset counts while cumulative, then Reset zoom at the right where it fits.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<HeatMapView::Button> HeatMapView::GetButtons() const
{
    float                wide  = m_scaler.ToPxf ((float) kWideTabDip);
    float                x     = (float) m_boundsDip.left + m_scaler.ToPxf ((float) kGutterDip);
    float                right = (float) m_boundsDip.right - m_scaler.ToPxf ((float) kInsetDip);
    std::vector<Button>  buttons;



    buttons.push_back ({ Action::Fading,     x,        wide });
    buttons.push_back ({ Action::Cumulative, x + wide, wide });
    buttons.push_back ({ m_options.cumulative ? Action::ResetCounts : Action::Fade, x + wide * 2, wide });
    buttons.push_back ({ Action::ResetZoom,  std::max (x + wide * 3, right - wide), wide });

    return buttons;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetActionAt
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapView::Action> HeatMapView::GetActionAt (POINT point) const
{
    long  bar = m_scaler.ToPx (kBarDip);



    if (point.y < m_boundsDip.top + bar || point.y >= m_boundsDip.top + bar * kBarRows)
    {
        return std::nullopt;
    }

    for (const Button & button : GetButtons())
    {
        if ((float) point.x >= button.left && (float) point.x < button.left + button.width)
        {
            return button.action;
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::RunAction
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::RunAction (Action action)
{
    switch (action)
    {
    case Action::Fading:      m_options.cumulative  = false;                                                  break;
    case Action::Cumulative:  m_options.cumulative  = true;                                                   break;
    case Action::Fade:        m_options.fadeSeconds = HeatMapOptions::GetNextFadeSeconds (m_options.fadeSeconds); break;

    case Action::ResetCounts:
        if (m_onResetCounts)
        {
            m_onResetCounts();
        }

        return;

    default:
        ResetZoom();
        return;
    }

    if (m_onOptionsChanged)
    {
        m_onOptionsChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetReadout
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapView::GetReadout() const
{
    constexpr size_t           kCount = (size_t) kSide * kSide;
    std::vector<std::wstring>  kinds;
    std::wstring               text;



    if (!m_hover.has_value())
    {
        return {};
    }

    text = std::format (L"${:04X}", *m_hover);

    if (m_execute.size() == kCount && m_execute[*m_hover] != 0) { kinds.push_back (L"executed"); }
    if (m_read.size()    == kCount && m_read[*m_hover]    != 0) { kinds.push_back (L"read");     }
    if (m_write.size()   == kCount && m_write[*m_hover]   != 0) { kinds.push_back (L"written");  }

    for (size_t i = 0; i < kinds.size(); i++)
    {
        text += (i == 0) ? L"  " : L", ";
        text += kinds[i];
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::Layout
//
//  The first layout sets the starting zoom, which depends on the dpi.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    m_boundsDip = boundsPx;
    m_scaler    = scaler;

    if (m_cellPx <= 0)
    {
        m_cellPx = std::max (1, m_scaler.ToPx (kDefaultCellDip));
    }

    ClampScroll();
    BuildFrame();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT     map    = GetMapRect();
    long     width  = map.right  - map.left;
    long     height = map.bottom - map.top;
    HRESULT  hr     = S_OK;



    if (!m_visible || m_boundsDip.right <= m_boundsDip.left)
    {
        return;
    }

    if (m_isFrameStale)
    {
        BuildFrame();
    }

    PaintBar     (painter, text, theme);
    PaintActions (painter, text, theme);
    PaintPages   (text, theme);

    if (width > 0 && height > 0 && m_frame.size() == (size_t) (width * height))
    {
        hr = text.DrawFramebuffer (m_frame.data(), (int) width, (int) height, (float) map.left, (float) map.top, (float) width, (float) height);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    PaintHover (text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::PaintBar
//
//  The modes as tabs, the chosen one underlined in the accent; then a swatch
//  for each color the mode shows, and the readout at the right.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintBar (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr int   kSwatchDip  = 10;
    constexpr int   kKeyDip     = 56;
    DxuiFontHandle  font        = theme.BodyFont();
    float           size        = m_scaler.ToPxf (font.sizeDip);
    float           bar         = m_scaler.ToPxf ((float) kBarDip);
    float           tab         = m_scaler.ToPxf ((float) kTabDip);
    float           swatch      = m_scaler.ToPxf ((float) kSwatchDip);
    float           key         = m_scaler.ToPxf ((float) kKeyDip);
    float           underline   = std::max (1.0f, m_scaler.ToPxf (2.0f));
    float           top         = (float) m_boundsDip.top;
    float           x           = (float) m_boundsDip.left + m_scaler.ToPxf ((float) kGutterDip);
    float           right       = (float) m_boundsDip.right - m_scaler.ToPxf ((float) kInsetDip);
    std::wstring    readout     = GetReadout();
    HRESULT         hr          = S_OK;
    KeyList         keys;



    for (int index = 0; index < kModeCount; index++)
    {
        Mode  mode     = (Mode) index;
        bool  isChosen = (mode == m_options.view);



        hr = text.DrawString (GetModeLabel (mode).c_str(), x, top, tab, bar, isChosen ? theme.Foreground() : theme.ForegroundMuted(),
                              size, font.face, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (isChosen)
        {
            painter.FillRect (x + tab / 4, top + bar - underline, tab / 2, underline, theme.Accent());
        }

        x += tab;
    }

    if (m_options.view != Mode::Data) { keys.emplace_back (L"Code",  m_palette.execute); }
    if (m_options.view != Mode::Code) { keys.emplace_back (L"Read",  m_palette.read);    }
    if (m_options.view != Mode::Code) { keys.emplace_back (L"Write", m_palette.write);   }

    x += swatch;

    for (const auto & [label, color] : keys)
    {
        if (x + key > right)
        {
            break;
        }

        painter.FillRect (x, top + (bar - swatch) / 2, swatch, swatch, color | 0xFF000000u);

        hr = text.DrawString (label.c_str(), x + swatch * 1.5f, top, key - swatch * 1.5f, bar, theme.ForegroundMuted(),
                              size, font.face, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        x += key;
    }

    if (!readout.empty() && right > x)
    {
        hr = text.DrawString (readout.c_str(), x, top, right - x, bar, theme.Foreground(),
                              size, theme.MonospaceFont().face, DxuiTextHAlign::Right, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::PaintActions
//
//  Fading and Cumulative as tabs, the chosen one underlined as a mode is; the
//  rest in the accent, as links are.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintActions (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    DxuiFontHandle  font      = theme.BodyFont();
    float           size      = m_scaler.ToPxf (font.sizeDip);
    float           bar       = m_scaler.ToPxf ((float) kBarDip);
    float           underline = std::max (1.0f, m_scaler.ToPxf (2.0f));
    float           top       = (float) m_boundsDip.top + bar;
    float           right     = (float) m_boundsDip.right - m_scaler.ToPxf ((float) kInsetDip);
    HRESULT         hr        = S_OK;



    for (const Button & button : GetButtons())
    {
        bool      isTab    = button.action == Action::Fading || button.action == Action::Cumulative;
        bool      isChosen = isTab && ((button.action == Action::Cumulative) == m_options.cumulative);
        uint32_t  color    = isTab ? (isChosen ? theme.Foreground() : theme.ForegroundMuted()) : theme.Accent();



        if (button.left + button.width > right)
        {
            continue;
        }

        hr = text.DrawString (GetActionLabel (button.action).c_str(), button.left, top, button.width, bar, color,
                              size, font.face, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (isChosen)
        {
            painter.FillRect (button.left + button.width / 4, top + bar - underline, button.width / 2, underline, theme.Accent());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::PaintPages
//
//  Page numbers beside their rows, every page while the rows are tall enough
//  to tell the labels apart and every second, fourth and so on below that.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintPages (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr float  kLineHeight = 1.4f;
    RECT             map         = GetMapRect();
    DxuiFontHandle   font        = theme.MonospaceFont();
    float            size        = m_scaler.ToPxf (font.sizeDip);
    long             pitch       = GetPitch();
    float            gutter      = m_scaler.ToPxf ((float) (kGutterDip - 4));
    long             step        = 1;
    long             page        = 0;
    float            y           = 0.0f;
    std::wstring     label;
    HRESULT          hr          = S_OK;



    if (pitch <= 0)
    {
        return;
    }

    while (step < kSide && (float) (step * pitch) < size * kLineHeight)
    {
        step *= 2;
    }

    //  The first labeled page at or below the area's top.
    page = ((m_scroll.y / pitch + step - 1) / step) * step;

    for (; page < kSide; page += step)
    {
        y = (float) (map.top + page * pitch - m_scroll.y);

        if (y + size > (float) map.bottom)
        {
            break;
        }

        label = std::format (L"${:02X}", page);

        hr = text.DrawString (label.c_str(), (float) m_boundsDip.left, y, gutter, size * kLineHeight,
                              theme.ForegroundMuted(), size, font.face, DxuiTextHAlign::Right, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::PaintHover
//
//  A frame around the cell the readout reports, drawn in the streets around
//  it, so a snap to a busier cell shows where it went.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintHover (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    RECT      map   = GetMapRect();
    long      pitch = GetPitch();
    float     left  = 0.0f;
    float     top   = 0.0f;
    float     side  = (float) (m_cellPx + 2 * kStreetPx);
    float     line  = (float) kStreetPx;
    uint32_t  color = theme.Foreground();
    HRESULT   hr    = S_OK;



    if (!m_hover.has_value())
    {
        return;
    }

    left = (float) (map.left + (long) (*m_hover & 0xFF) * pitch - m_scroll.x - kStreetPx);
    top  = (float) (map.top  + (long) (*m_hover >> 8)   * pitch - m_scroll.y - kStreetPx);

    if (left + side <= (float) map.left || left >= (float) map.right || top + side <= (float) map.top || top >= (float) map.bottom)
    {
        return;
    }

    hr = text.FillRect (left,               top,               side, line, color);
    IGNORE_RETURN_VALUE (hr, S_OK);
    hr = text.FillRect (left,               top + side - line, side, line, color);
    IGNORE_RETURN_VALUE (hr, S_OK);
    hr = text.FillRect (left,               top,               line, side, color);
    IGNORE_RETURN_VALUE (hr, S_OK);
    hr = text.FillRect (left + side - line, top,               line, side, color);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnPress
//
//  A press on a mode or an action carries it out; one on the map starts what
//  is a click or a drag, which the release and the moves decide.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnPress (const DxuiMouseEvent & ev)
{
    std::optional<Mode>    mode   = GetModeAt   (ev.positionDip);
    std::optional<Action>  action = GetActionAt (ev.positionDip);



    if (mode.has_value())
    {
        SetMode (*mode);

        if (m_onOptionsChanged)
        {
            m_onOptionsChanged();
        }

        return true;
    }

    if (action.has_value())
    {
        RunAction (*action);
        return true;
    }

    if (!GetAddressAt (ev.positionDip).has_value())
    {
        return false;
    }

    m_press       = ev.positionDip;
    m_pressScroll = m_scroll;
    m_isDragging  = false;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnRelease
//
//  A press that never became a drag is a click on the cell it picked.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnRelease (const DxuiMouseEvent & ev)
{
    std::optional<Word>  picked;



    if (!m_press.has_value())
    {
        return false;
    }

    if (!m_isDragging)
    {
        picked = GetPickAt (*m_press);
    }

    m_press.reset();
    m_isDragging = false;
    m_hover      = GetPickAt (ev.positionDip);

    if (picked.has_value() && m_onPickAddress)
    {
        m_onPickAddress (*picked);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnDragOrHover
//
//  With the button down, once the mouse has gone kDragDip the map follows
//  it; otherwise the readout follows the mouse.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnDragOrHover (const DxuiMouseEvent & ev)
{
    long  dx = 0;
    long  dy = 0;



    if (!m_press.has_value())
    {
        m_hover = GetPickAt (ev.positionDip);
        return false;
    }

    dx = ev.positionDip.x - m_press->x;
    dy = ev.positionDip.y - m_press->y;

    if (!m_isDragging && std::max (std::abs (dx), std::abs (dy)) >= m_scaler.ToPx (kDragDip))
    {
        m_isDragging = true;
        m_hover.reset();
    }

    if (!m_isDragging)
    {
        return true;
    }

    m_scroll = { m_pressScroll.x - dx, m_pressScroll.y - dy };

    ClampScroll();
    BuildFrame();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnMouse
//
//  The wheel over the map zooms about the mouse; with Shift it scrolls up and
//  down, and a sideways wheel scrolls across.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnMouse (const DxuiMouseEvent & ev)
{
    constexpr int  kCellsPerNotch = 8;
    long           notch          = (long) std::lround (ev.wheelDelta * (float) (kCellsPerNotch * GetPitch()));



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Leave:
        if (!m_press.has_value())
        {
            m_hover.reset();
        }

        return false;

    case DxuiMouseEventKind::Move:
        return OnDragOrHover (ev);

    case DxuiMouseEventKind::Down:
        return ev.button == DxuiMouseButton::Left && OnPress (ev);

    case DxuiMouseEventKind::Up:
        return ev.button == DxuiMouseButton::Left && OnRelease (ev);

    case DxuiMouseEventKind::Wheel:
        break;

    default:
        return false;
    }

    if (!GetAddressAt (ev.positionDip).has_value() || ev.wheelDelta == 0.0f)
    {
        return false;
    }

    if (ev.wheelHorizontal)
    {
        ScrollBy ((int) notch, 0);
    }
    else if (ev.shift)
    {
        ScrollBy (0, (int) -notch);
    }
    else
    {
        ZoomAt (ev.positionDip, ev.wheelDelta);
    }

    m_hover = GetPickAt (ev.positionDip);
    return true;
}





