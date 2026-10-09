#include "Pch.h"

#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/ColorKeyPopup.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ColorKeyPopup::MakeRows
//
//  A text color's swatch is a sample of text and a row fill's a sample on
//  the fill, as the legend dialog drew them; every other shape is drawn as
//  an image once, here.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<ColorKeyPopup::Row> ColorKeyPopup::MakeRows (ColorLegend::Pane pane, const ColorLegend::Palette & palette)
{
    std::vector<Row>  rows;



    for (const ColorLegend::Entry & entry : ColorLegend::GetEntriesFor (pane))
    {
        Row  row;

        row.swatch = entry.swatch;
        row.argb   = ColorLegend::GetArgb (entry.meaning, palette);
        row.text   = ColorLegend::GetText (entry.meaning);

        if (entry.swatch != ColorLegend::Swatch::Row && entry.swatch != ColorLegend::Swatch::Text && entry.swatch != ColorLegend::Swatch::Italic &&
            entry.swatch != ColorLegend::Swatch::Marker)
        {
            row.icon = ColorLegend::MakeSwatchIcon (entry.swatch, row.argb);
        }

        rows.push_back (std::move (row));
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorKeyPopup::Show
//
//  Placed below the button, or above it where below would leave the screen,
//  as a tip is: no activation, and the pointer passes through to the button
//  under it. A key already up moves and is drawn again in place.
//
////////////////////////////////////////////////////////////////////////////////

void ColorKeyPopup::Show (
    DxuiHwndSource              * host,
    const RECT                  & anchor,
    ColorLegend::Pane             pane,
    const ColorLegend::Palette  & palette,
    const IDxuiTheme            & theme,
    bool                          hold)
{
    constexpr float            kBorderScale = 0.5f;   // the border at half the fill's brightness



    HRESULT                    hr       = S_OK;
    POINT                      topLeft  = { anchor.left,  anchor.top    };
    POINT                      botRight = { anchor.right, anchor.bottom };
    HWND                       owner    = nullptr;
    RECT                       screen   = {};
    bool                       hasHost  = host != nullptr;



    if (m_popup != nullptr && m_host != host)
    {
        Hide();
    }

    m_host       = host;
    m_pane       = pane;
    m_isHeld     = hold;
    m_isShown    = true;
    m_rows       = MakeRows (pane, palette);
    m_background = theme.ContentBackground();
    m_border     = DxuiColor::Darken (m_background, kBorderScale);
    m_foreground = theme.Foreground();

    //  A window with no popup host, as in a test, keeps the key's state
    //  without a balloon to draw it in.
    BAIL_OUT_IF (!hasHost, S_OK);

    m_scaler.SetDpi (host->GetScaler().GetDpi());

    owner = host->GetHwnd();

    if (owner != nullptr)
    {
        ClientToScreen (owner, &topLeft);
        ClientToScreen (owner, &botRight);
    }

    screen = { topLeft.x, topLeft.y, botRight.x, botRight.y };

    if (m_popup != nullptr)
    {
        hr = m_popup->MoveTo (screen, MeasureDip (m_scaler.GetDpi()));
    }
    else
    {
        hr = Raise (owner, screen);
    }

    CHR (hr);

Error:
    if (FAILED (hr))
    {
        Hide();
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorKeyPopup::Raise
//
//  A balloon from the host's pool, sized to the rows and shown against the
//  anchor.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ColorKeyPopup::Raise (HWND owner, const RECT & screen)
{
    HRESULT                    hr     = S_OK;
    DxuiPopupHost::ShowParams  params;



    m_popup = m_host->AcquirePopup();
    CBR (m_popup != nullptr);

    params.ownerHwnd        = owner;
    params.anchorRectScreen = screen;
    params.placement        = DxuiPopupPlacement::Below;
    params.flipIfOffscreen  = true;
    params.dismiss          = DxuiPopupDismiss::Manual;
    params.input            = DxuiPopupInput::PassThrough;
    params.shadow           = true;
    params.sizeDip          = MeasureDip (m_scaler.GetDpi());
    params.backgroundArgb   = m_background;
    params.renderContent    = [this] (IDxuiPainter & p, IDxuiTextRenderer & t) { Render (p, t); };
    params.onClosed         = [this] () { m_popup = nullptr; m_isShown = false; };

    hr = m_popup->Show (std::move (params));
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorKeyPopup::Hide
//
////////////////////////////////////////////////////////////////////////////////

void ColorKeyPopup::Hide()
{
    DxuiPopupHost  * popup = m_popup;



    //  Cleared first, so the popup's closing callback finds nothing to undo.
    m_popup   = nullptr;
    m_isShown = false;
    m_isHeld  = false;

    if (popup != nullptr && m_host != nullptr)
    {
        m_host->ReleasePopup (popup);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorKeyPopup::MeasureDip
//
//  The widest meaning beside a swatch, and a row for each, inside the
//  padding. Measured in the pixels it is drawn in, on the popup's own
//  renderer, or estimated where there is none (a test).
//
////////////////////////////////////////////////////////////////////////////////

SIZE ColorKeyPopup::MeasureDip (UINT dpi) const
{
    constexpr float  kEstCharWidthEm = 0.6f;
    HRESULT          hr              = S_OK;
    float            fontPx          = m_scaler.ToPxf (kFontDip);
    float            widest          = 0.0f;
    float            widthPx         = 0.0f;
    float            heightPx        = 0.0f;
    UINT             scale           = (dpi == 0) ? (UINT) DxuiDpiScaler::kBaseDpi : dpi;



    for (const Row & row : m_rows)
    {
        float  w = 0.0f;
        float  h = 0.0f;

        hr = (m_popup != nullptr) ? m_popup->MeasureText (row.text.c_str(), fontPx, DxuiTheme::kBodyFace, w, h) : E_FAIL;

        if (FAILED (hr) || w <= 0.0f)
        {
            w = (float) row.text.size() * fontPx * kEstCharWidthEm;
        }

        widest = std::max (widest, std::min (w, m_scaler.ToPxf ((float) kMaxTextDip)));
    }

    widthPx  = m_scaler.ToPxf ((float) (kPadDip * 2 + kSwatchDip + kGapDip)) + std::ceil (widest);
    heightPx = m_scaler.ToPxf ((float) (kPadDip * 2)) + m_scaler.ToPxf ((float) kRowDip) * (float) m_rows.size();

    return SIZE { (LONG) std::ceil (widthPx  * (float) DxuiDpiScaler::kBaseDpi / (float) scale),
                  (LONG) std::ceil (heightPx * (float) DxuiDpiScaler::kBaseDpi / (float) scale) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorKeyPopup::Render
//
//  Popup-local pixels. Each row is its swatch, centered on the row, then its
//  meaning; the border is drawn over the host's clear to the background.
//
////////////////////////////////////////////////////////////////////////////////

void ColorKeyPopup::Render (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    constexpr const wchar_t  * kSample   = L"Ab";
    constexpr float            kSampleEm = 0.75f;
    HRESULT                    hr        = S_OK;
    RECT                       placed    = {};
    float                      pad       = m_scaler.ToPxf ((float) kPadDip);
    float                      swatch    = m_scaler.ToPxf ((float) kSwatchDip);
    float                      gap       = m_scaler.ToPxf ((float) kGapDip);
    float                      rowPx     = m_scaler.ToPxf ((float) kRowDip);
    float                      fontPx    = m_scaler.ToPxf (kFontDip);
    float                      width     = 0.0f;
    float                      height    = 0.0f;



    if (m_popup == nullptr)
    {
        return;
    }

    placed = m_popup->GetPlacedRectScreenPx();
    width  = (float) (placed.right  - placed.left);
    height = (float) (placed.bottom - placed.top);

    painter.OutlineRoundedRect (0.0f, 0.0f, width, height, m_scaler.ToPxf (DxuiTheme::kOverlayCornerRadiusDip), 1.0f, m_border);

    for (size_t i = 0; i < m_rows.size(); i++)
    {
        const Row  & row  = m_rows[i];
        float        top  = pad + rowPx * (float) i;
        float        boxY = top + (rowPx - swatch) / 2.0f;
        float        left = pad + swatch + gap;

        switch (row.swatch)
        {
        case ColorLegend::Swatch::Row:
            hr = text.FillRect (pad, boxY, swatch, swatch, row.argb);
            IGNORE_RETURN_VALUE (hr, S_OK);
            hr = text.DrawString (kSample, pad, boxY, swatch, swatch, m_foreground, fontPx * kSampleEm, DxuiTheme::kBodyFace,
                                  DxuiTextHAlign::Center, DxuiTextVAlign::Center);
            break;

        case ColorLegend::Swatch::Text:
            hr = text.DrawString (kSample, pad, boxY, swatch, swatch, row.argb, fontPx * kSampleEm, DxuiTheme::kBodyFace,
                                  DxuiTextHAlign::Center, DxuiTextVAlign::Center);
            break;

        case ColorLegend::Swatch::Italic:
            hr = text.DrawString (kSample, pad, boxY, swatch, swatch, m_foreground, fontPx * kSampleEm, DxuiTheme::kBodyFace,
                                  DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Italic);
            break;

        case ColorLegend::Swatch::Marker:
            hr = text.DrawString (s_kpszTriangleRight, pad, boxY, swatch, swatch, row.argb, fontPx, DxuiTheme::kBodyFace,
                                  DxuiTextHAlign::Center, DxuiTextVAlign::Center);
            break;

        default:
            hr = (row.icon != nullptr) ? text.DrawIconBitmap (row.icon->bgraPremul.data(), row.icon->width, row.icon->height, pad, boxY, swatch, swatch) : S_OK;
            break;
        }

        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (row.text.c_str(), left, top, width - left - pad, rowPx, m_foreground, fontPx, DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}
