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

    BuildPixels();
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

    BuildPixels();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetMode
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetMode (Mode mode)
{
    m_mode = mode;

    BuildPixels();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetColor
//
//  An address takes the color of the kind that touched it most, as the mode
//  lets it be seen, mixed toward that color from the background by how hot it
//  is. Where code and data are equally hot, code shows. A write shows over a
//  read as hot.
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
        return palette.background | 0xFF000000u;
    }

    return DxuiColor::Mix (palette.background | 0xFF000000u, hue | 0xFF000000u,
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
//  HeatMapView::BuildPixels
//
//  The bitmap the map draws: row by page, column by byte in the page. A kind
//  with no levels is cold everywhere.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::BuildPixels()
{
    constexpr size_t  kCount     = (size_t) kSide * kSide;
    bool              hasExecute = m_execute.size() == kCount;
    bool              hasRead    = m_read.size()    == kCount;
    bool              hasWrite   = m_write.size()   == kCount;



    m_pixels.resize (kCount);

    for (size_t address = 0; address < kCount; address++)
    {
        m_pixels[address] = GetColor (m_mode,
                                      hasExecute ? m_execute[address] : (Byte) 0,
                                      hasRead    ? m_read[address]    : (Byte) 0,
                                      hasWrite   ? m_write[address]   : (Byte) 0,
                                      m_palette);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetMapRect
//
//  Below the bar and right of the page numbers, filling the rest of the pane
//  but a margin, so every address has as many pixels as the pane can give it.
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapView::GetMapRect() const
{
    RECT  map = m_boundsDip;



    map.left  += m_scaler.ToPx (kGutterDip);
    map.top   += m_scaler.ToPx (kBarDip);
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
    long  width  = map.right  - map.left;
    long  height = map.bottom - map.top;
    long  page   = 0;
    long  offset = 0;



    if (width <= 0 || height <= 0 || point.x < map.left || point.x >= map.right || point.y < map.top || point.y >= map.bottom)
    {
        return std::nullopt;
    }

    page   = (point.y - map.top)  * kSide / height;
    offset = (point.x - map.left) * kSide / width;

    return (Word) ((page << 8) | offset);
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
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    m_boundsDip = boundsPx;
    m_scaler    = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT     map = GetMapRect();
    HRESULT  hr  = S_OK;



    if (!m_visible || m_boundsDip.right <= m_boundsDip.left)
    {
        return;
    }

    PaintBar   (painter, text, theme);
    PaintPages (text, theme);

    if (map.right > map.left && !m_pixels.empty())
    {
        hr = text.DrawFramebuffer (m_pixels.data(), kSide, kSide, (float) map.left, (float) map.top,
                                   (float) (map.right - map.left), (float) (map.bottom - map.top));
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
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
        bool  isChosen = (mode == m_mode);



        hr = text.DrawString (GetModeLabel (mode).c_str(), x, top, tab, bar, isChosen ? theme.Foreground() : theme.ForegroundMuted(),
                              size, font.face, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (isChosen)
        {
            painter.FillRect (x + tab / 4, top + bar - underline, tab / 2, underline, theme.Accent());
        }

        x += tab;
    }

    if (m_mode != Mode::Data) { keys.emplace_back (L"Code",  m_palette.execute); }
    if (m_mode != Mode::Code) { keys.emplace_back (L"Read",  m_palette.read);    }
    if (m_mode != Mode::Code) { keys.emplace_back (L"Write", m_palette.write);   }

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
//  HeatMapView::PaintPages
//
//  Every kLabelPages-th page's number beside its row, where the rows are tall
//  enough to tell the labels apart.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::PaintPages (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    RECT            map    = GetMapRect();
    DxuiFontHandle  font   = theme.MonospaceFont();
    float           size   = m_scaler.ToPxf (font.sizeDip);
    float           row    = (float) (map.bottom - map.top) / (float) kSide;
    float           gutter = m_scaler.ToPxf ((float) (kGutterDip - 4));
    float           step   = row * (float) kLabelPages;
    std::wstring    label;
    HRESULT         hr     = S_OK;



    if (step < size)
    {
        return;
    }

    for (int page = 0; page < kSide; page += kLabelPages)
    {
        label = std::format (L"${:02X}", page);

        hr = text.DrawString (label.c_str(), (float) m_boundsDip.left, (float) map.top + row * (float) page, gutter, size * 1.4f,
                              theme.ForegroundMuted(), size, font.face, DxuiTextHAlign::Right, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnMouse
//
//  A click on a mode shows it; moving over the map shows the address under
//  the mouse in the readout.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnMouse (const DxuiMouseEvent & ev)
{
    std::optional<Mode>  mode;



    if (ev.kind == DxuiMouseEventKind::Leave)
    {
        m_hover.reset();
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Move)
    {
        m_hover = GetAddressAt (ev.positionDip);
        return false;
    }

    if (ev.kind != DxuiMouseEventKind::Down || ev.button != DxuiMouseButton::Left)
    {
        return false;
    }

    mode = GetModeAt (ev.positionDip);

    if (!mode.has_value())
    {
        return false;
    }

    SetMode (*mode);
    return true;
}





