#include "Pch.h"

#include "DxuiCaptionBar.h"
#include "Core/DxuiTextElide.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCaptionBar
//
//  Constructor — defers entirely to DxuiPanel; host-owned rendering is
//  opt-in via ConfigureButtons.
//
////////////////////////////////////////////////////////////////////////////////

DxuiCaptionBar::DxuiCaptionBar()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigureButtons
//
//  Builds the owned min / max / close DxuiSystemButton children for the
//  requested layout and switches the bar into self-rendering mode
//  (gradient + icon + title). Idempotent enough for construction-time
//  use; not intended to be toggled at runtime.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCaptionBar::ConfigureButtons (Buttons buttons)
{
    DXUI_ASSERT_UI_THREAD();

    // Drop the OLD buttons out of the child list before replacing them. The
    // unique_ptr assignments below destroy whatever is there, while the panel
    // keeps NON-OWNING pointers from Adopt -- so without this a second call
    // leaves the list pointing at freed controls and the next Layout walks
    // into one. Adopt's own contract says as much; this is that contract
    // being kept, so re-configuring is merely wasteful rather than fatal.
    {
        HRESULT  hrRemove = S_OK;

        for (DxuiSystemButton * btn : { m_minBtn.get(), m_maxBtn.get(), m_closeBtn.get() })
        {
            if (btn != nullptr)
            {
                hrRemove = RemoveAdopted (*btn);
                IGNORE_RETURN_VALUE (hrRemove, S_OK);
            }
        }
    }

    m_buttons       = buttons;
    m_renderCaption = true;
    m_minBtn.reset();
    m_maxBtn.reset();
    m_closeBtn.reset();

    if (buttons == Buttons::MinMaxClose)
    {
        m_minBtn = std::make_unique<DxuiSystemButton> (DxuiSystemButtonKind::Min);
        Adopt (*m_minBtn);
    }

    if (buttons == Buttons::MinMaxClose || buttons == Buttons::MaxClose)
    {
        m_maxBtn = std::make_unique<DxuiSystemButton> (DxuiSystemButtonKind::Max);
        Adopt (*m_maxBtn);
    }

    if (buttons != Buttons::None)
    {
        m_closeBtn = std::make_unique<DxuiSystemButton> (DxuiSystemButtonKind::Close);
        Adopt (*m_closeBtn);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSystemHwnd
//
//  Hands the owning window's HWND to every system button so each can
//  dispatch its standard Win32 system command on click.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCaptionBar::SetSystemHwnd (HWND hwnd)
{
    DXUI_ASSERT_UI_THREAD();

    if (m_minBtn)   { m_minBtn->SetHwnd (hwnd);   }
    if (m_maxBtn)   { m_maxBtn->SetHwnd (hwnd);   }
    if (m_closeBtn) { m_closeBtn->SetHwnd (hwnd); }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTitle
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCaptionBar::SetTitle (const std::wstring & title, size_t markAt)
{
    DXUI_ASSERT_UI_THREAD();

    m_title  = title;
    m_markAt = (markAt <= title.size()) ? markAt : std::wstring::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetAppIcon
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCaptionBar::SetAppIcon (std::vector<uint32_t> bgraPremul, int widthPx, int heightPx)
{
    DXUI_ASSERT_UI_THREAD();

    m_iconPixels = std::move (bgraPremul);
    m_iconW      = widthPx;
    m_iconH      = heightPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMaximized
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCaptionBar::SetMaximized (bool maximized)
{
    DXUI_ASSERT_UI_THREAD();

    m_maximized = maximized;
    if (m_maxBtn)
    {
        m_maxBtn->SetMaximized (maximized);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetAccessory
//
//  Adopts the consumer's control into the caption so it paints over the
//  gradient and takes part in hit classification. The previous one, if any,
//  is released first; the caption never owns either.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCaptionBar::SetAccessory (IDxuiControl * accessory)
{
    HRESULT  hrRemove = S_OK;



    DXUI_ASSERT_UI_THREAD();

    if (m_accessory != nullptr)
    {
        hrRemove = RemoveAdopted (*m_accessory);
        IGNORE_RETURN_VALUE (hrRemove, S_OK);
    }

    m_accessory = accessory;

    if (m_accessory != nullptr)
    {
        Adopt (*m_accessory);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPreferredHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiCaptionBar::GetPreferredHeightPx (const DxuiDpiScaler & scaler) const
{
    return scaler.ToPx (kCaptionHeightDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPreferredHeightDip
//
////////////////////////////////////////////////////////////////////////////////

int DxuiCaptionBar::GetPreferredHeightDip() const
{
    return kCaptionHeightDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHeightPxForDpi
//
////////////////////////////////////////////////////////////////////////////////

int DxuiCaptionBar::GetHeightPxForDpi (UINT dpi)
{
    constexpr int      kBaseDpi          = 96;



    return MulDiv (kCaptionHeightDip, (int) dpi, kBaseDpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  Captures the scaler and right-aligns the system buttons in DIP
//  space (each button is one fixed-width column at full caption
//  height). DxuiPanel::Layout only recurses into child panels, so the
//  non-panel buttons must be laid out explicitly here.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCaptionBar::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int   right  = 0;
    int   top    = 0;
    int   bottom = 0;
    RECT  rc     = {};



    DXUI_ASSERT_UI_THREAD();

    DxuiPanel::Layout (boundsDip, scaler);
    m_scaler = scaler;

    right  = boundsDip.right;
    top    = boundsDip.top;
    bottom = boundsDip.bottom;

    if (m_closeBtn)
    {
        rc = { right - kButtonWidthDip, top, right, bottom };
        m_closeBtn->Layout (rc, scaler);
        right -= kButtonWidthDip;
    }

    if (m_maxBtn)
    {
        rc = { right - kButtonWidthDip, top, right, bottom };
        m_maxBtn->Layout (rc, scaler);
        right -= kButtonWidthDip;
    }

    if (m_minBtn)
    {
        rc = { right - kButtonWidthDip, top, right, bottom };
        m_minBtn->Layout (rc, scaler);
        right -= kButtonWidthDip;
    }

    if (m_accessory != nullptr)
    {
        rc = { right - m_accessoryWidthDip, top, right, bottom };
        m_accessory->Layout (rc, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTooltipAt
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DxuiCaptionBar::GetTooltipAt (POINT clientPx, RECT & anchorPx) const
{
    if (!m_titleCut || !PtInRect (&m_titleRectPx, clientPx))
    {
        return nullptr;
    }

    anchorPx = m_titleRectPx;

    return m_title.c_str();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  Host-owned mode: gradient fill + app icon + title text, then the
//  base panel walk paints the system-button children on top. Plain-
//  container mode (m_renderCaption == false) paints only the children
//  so a plain-container caller's layout is unchanged.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCaptionBar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr float    kTitleFontDip     = 14.0f;
    constexpr float    kTitlePadDip      = 14.0f;
    constexpr float    kIconPadFraction  = 0.18f;



    RECT          b             = {};
    float         xPx           = 0.0f;
    float         yPx           = 0.0f;
    float         wPx           = 0.0f;
    float         hPx           = 0.0f;
    float         iconPadPx     = 0.0f;
    float         iconSizePx    = 0.0f;
    float         textLeftPx    = 0.0f;
    float         textOffsetPx  = 0.0f;
    float         titleWidthPx  = 0.0f;
    float         fontPx        = 0.0f;
    float         buttonStripPx = 0.0f;
    int           buttonCount   = 0;



    DXUI_ASSERT_UI_THREAD();

    if (!m_renderCaption)
    {
        m_titleCut = false;
        DxuiPanel::Paint (painter, text, theme);
        return;
    }

    b   = GetBounds();
    xPx = (float) m_scaler.ToPx (b.left);
    yPx = (float) m_scaler.ToPx (b.top);
    wPx = (float) m_scaler.ToPx (b.right - b.left);
    hPx = (float) m_scaler.ToPx (b.bottom - b.top);

    painter.FillGradientRect (xPx, yPx, wPx, hPx, theme.TitleBarTop(), theme.TitleBarBottom());

    iconPadPx    = hPx * kIconPadFraction;
    iconSizePx   = hPx - iconPadPx * 2.0f;
    textOffsetPx = m_scaler.ToPxf (kTitlePadDip);

    if (iconSizePx > 0.0f && !m_iconPixels.empty() && m_iconW > 0 && m_iconH > 0)
    {
        HRESULT  hrIcon = text.DrawIconBitmap (m_iconPixels.data(),
                                               m_iconW, m_iconH,
                                               xPx + textOffsetPx,
                                               yPx + iconPadPx,
                                               iconSizePx, iconSizePx);
        IGNORE_RETURN_VALUE (hrIcon, S_OK);
        textOffsetPx = m_scaler.ToPxf (kTitlePadDip) + iconSizePx + iconPadPx;
    }

    buttonCount   = (m_minBtn ? 1 : 0) + (m_maxBtn ? 1 : 0) + (m_closeBtn ? 1 : 0);
    buttonStripPx = (float) buttonCount * m_scaler.ToPxf ((float) kButtonWidthDip);
    buttonStripPx += (m_accessory != nullptr && m_accessory->IsVisible()) ? m_scaler.ToPxf ((float) m_accessoryWidthDip) : 0.0f;

    textLeftPx   = xPx + textOffsetPx;
    titleWidthPx = wPx - textOffsetPx - buttonStripPx - m_scaler.ToPxf (kTitlePadDip);
    if (titleWidthPx < 0.0f)
    {
        titleWidthPx = 0.0f;
    }

    fontPx = m_scaler.ToPxf (kTitleFontDip);

    PaintTitle (painter, text, theme, RECT { std::lround (textLeftPx), std::lround (yPx), std::lround (textLeftPx + titleWidthPx), std::lround (yPx + hPx) }, fontPx);

    DxuiPanel::Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintTitle
//
//  One line, as Windows draws a caption. The unsaved-changes dot at
//  m_markAt always has its room. A title too long for the rest is cut in
//  the middle with an ellipsis: first the text before the dot, then, if
//  the title is too wide after that, the text after it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCaptionBar::PaintTitle (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & areaPx, float fontPx)
{
    constexpr float  kDotFraction = 0.45f;
    constexpr float  kGapFraction = 0.35f;



    bool          isMarked = m_markAt != std::wstring::npos;
    float         leftPx   = (float) areaPx.left;
    float         topPx    = (float) areaPx.top;
    float         widthPx  = (float) (areaPx.right - areaPx.left);
    float         heightPx = (float) (areaPx.bottom - areaPx.top);
    float         dotPx    = fontPx * kDotFraction;
    float         markPx   = isMarked ? fontPx * kGapFraction + dotPx : 0.0f;
    float         roomPx   = std::max (0.0f, widthPx - markPx);
    std::wstring  head     = isMarked ? m_title.substr (0, m_markAt) : m_title;
    std::wstring  tail     = isMarked ? m_title.substr (m_markAt) : std::wstring();
    float         headPx   = MeasureTitle (text, head, fontPx);
    float         tailPx   = MeasureTitle (text, tail, fontPx);
    HRESULT       hr       = S_OK;



    if (headPx + tailPx > roomPx)
    {
        head   = FitTitle (text, head, fontPx, roomPx - tailPx);
        headPx = MeasureTitle (text, head, fontPx);
        tail   = FitTitle (text, tail, fontPx, roomPx - headPx);
    }

    m_titleCut    = head + tail != m_title;
    m_titleRectPx = areaPx;

    hr = text.DrawString (head.c_str(), leftPx, topPx, widthPx, heightPx, theme.CaptionForeground(), fontPx, kTitleFamily, DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (isMarked)
    {
        painter.FillCircle (leftPx + headPx + markPx - dotPx / 2.0f, topPx + heightPx / 2.0f, dotPx / 2.0f, theme.ErrorForeground());

        hr = text.DrawString (tail.c_str(), leftPx + headPx + markPx, topPx, std::max (0.0f, widthPx - headPx - markPx), heightPx, theme.CaptionForeground(), fontPx, kTitleFamily, DxuiTextHAlign::Left, DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureTitle
//
////////////////////////////////////////////////////////////////////////////////

float DxuiCaptionBar::MeasureTitle (IDxuiTextRenderer & text, const std::wstring & value, float fontPx)
{
    float    widthPx  = 0.0f;
    float    heightPx = 0.0f;
    HRESULT  hr       = S_OK;



    if (!value.empty())
    {
        hr = text.MeasureString (value.c_str(), fontPx, kTitleFamily, widthPx, heightPx);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    return widthPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FitTitle
//
//  Part of the title cut in the middle to the room, or only the ellipsis
//  when the room is narrower than that.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiCaptionBar::FitTitle (IDxuiTextRenderer & text, const std::wstring & value, float fontPx, float roomPx)
{
    static constexpr wchar_t  kEllipsis[] = L"\x2026";



    std::wstring  fitted = value;



    if (!value.empty() && MeasureTitle (text, value, fontPx) > roomPx)
    {
        fitted = (roomPx <= MeasureTitle (text, kEllipsis, fontPx)) ? std::wstring (kEllipsis)
                                                                    : DxuiTextElide::ToWidth (text, value, fontPx, kTitleFamily, roomPx, DxuiElide::Middle);
    }

    return fitted;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClassifyHit
//
//  Front-to-back walk of children: the topmost child whose bounds
//  contain the point gets to classify. Blank caption area (no child
//  consumed it) falls through to DxuiHitTestKind::Caption.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHitTestKind DxuiCaptionBar::ClassifyHit (POINT clientDip) const
{
    IDxuiControl *   child      = nullptr;
    DxuiHitTestKind  kind       = DxuiHitTestKind::Caption;
    DxuiHitTestKind  childKind  = DxuiHitTestKind::None;
    RECT             rc         = {};
    size_t           n          = 0;
    size_t           i          = 0;
    bool             classified = false;



    n = GetChildCount();

    // Reverse order so visually-topmost children win. A child that reports
    // None declines the point and the walk continues past it, which is how a
    // transparent region inside a child's bounds still reads as Caption.
    //
    // `classified` rather than testing `kind`: a child may legitimately
    // answer Caption, and that has to STOP the walk like any other answer.
    for (i = n; !classified && i > 0; --i)
    {
        child = GetChild (i - 1);

        if (child != nullptr && child->IsVisible())
        {
            rc = child->GetBounds();

            if (clientDip.x >= rc.left && clientDip.x < rc.right &&
                clientDip.y >= rc.top  && clientDip.y < rc.bottom)
            {
                childKind  = child->ClassifyHit (clientDip);
                classified = (childKind != DxuiHitTestKind::None);

                if (classified)
                {
                    kind = childKind;
                }
            }
        }
    }

    return kind;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetReservedWidthDip
//
//  What the caption keeps for everything but the title and the accessory:
//  the system buttons, and the icon with the padding around it. A consumer
//  sizing its accessory subtracts this from the caption width.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiCaptionBar::GetReservedWidthDip() const
{
    constexpr int  kIconAndPadDip = 56;



    int  buttons = (m_minBtn ? 1 : 0) + (m_maxBtn ? 1 : 0) + (m_closeBtn ? 1 : 0);



    return buttons * kButtonWidthDip + kIconAndPadDip;
}