#include "Pch.h"

#include "DxuiStripFlyout.h"
#include "Core/DxuiIconImage.h"
#include "Theme/DxuiTheme.h"
#include "Window/DxuiHwndSource.h"
#include "Window/DxuiPopupHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripFlyout::~DxuiStripFlyout
//
////////////////////////////////////////////////////////////////////////////////

DxuiStripFlyout::~DxuiStripFlyout()
{
    Close();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripFlyout::SetPopupHost
//
//  A flyout open on the old host closes.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripFlyout::SetPopupHost (DxuiHwndSource * host)
{
    if (host != m_popupHost)
    {
        Close();
    }

    m_popupHost = host;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripFlyout::Track
//
//  Resting on the same mark goes on counting toward the open; a move to
//  another mark starts counting again, and closes a flyout open for the
//  last one. A move off every mark closes it, unless the pointer went into
//  it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripFlyout::Track (
    IDxuiImageStripSource  * source,
    bool                     isOverMark,
    uint64_t                 markId,
    const RECT             & anchorScreen,
    int64_t                  nowMs)
{
    bool  isSameMark = isOverMark && m_isOverMark && markId == m_markId;



    m_source = source;

    if (isSameMark)
    {
        return;
    }

    if (!isOverMark && m_isOpen && IsPointerOn())
    {
        return;
    }

    Close();

    m_isOverMark = isOverMark;
    m_markId     = markId;
    m_anchor     = anchorScreen;
    m_restSince  = nowMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripFlyout::Sync
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripFlyout::Sync (int64_t nowMs)
{
    std::vector<DxuiStripForkRow>  rows;
    bool                           isChanged = false;



    if (m_source == nullptr)
    {
        return;
    }

    if (!m_isOpen && m_isOverMark && nowMs - m_restSince >= kOpenDelayMs)
    {
        Open();
        return;
    }

    if (!m_isOpen)
    {
        return;
    }

    m_source->GetForkRows (m_markId, rows);

    isChanged = rows.size() != m_rows.size();

    for (size_t i = 0; i < rows.size() && !isChanged; i++)
    {
        isChanged = rows[i].id != m_rows[i].id || rows[i].picture != m_rows[i].picture;
    }

    if (!isChanged)
    {
        return;
    }

    m_rows.swap (rows);

    if (m_rows.empty())
    {
        Close();
    }
    else if (m_popup != nullptr)
    {
        m_popup->MarkDirty();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripFlyout::Open
//
//  The rows are taken from the source; with none, nothing opens. Without a
//  popup host, as in tests, the flyout is open with no window.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripFlyout::Open()
{
    DxuiPopupHost::ShowParams  params;
    HRESULT                    hr = S_OK;



    m_source->GetForkRows (m_markId, m_rows);

    if (m_rows.empty())
    {
        m_isOverMark = false;
        return;
    }

    m_isOpen     = true;
    m_hoveredRow = -1;

    if (m_popupHost == nullptr)
    {
        return;
    }

    m_popup = m_popupHost->AcquirePopup();

    if (m_popup == nullptr)
    {
        return;
    }

    params.ownerHwnd        = m_popupHost->GetHwnd();
    params.anchorRectScreen = m_anchor;
    params.placement        = DxuiPopupPlacement::Above;
    params.flipIfOffscreen  = true;
    params.dismiss          = DxuiPopupDismiss::OnPointerLeave;
    params.input            = DxuiPopupInput::Interactive;
    params.shadow           = true;
    params.grabsCapture     = false;
    params.sizeDip          = GetSizeDip();
    params.renderContent    = [this] (IDxuiPainter & painter, IDxuiTextRenderer & text) { Render (painter, text); };
    params.onMoveInside     = [this] (POINT localPx) { HoverAt (localPx); };
    params.onClickInside    = [this] (POINT localPx) { ClickAt (localPx); };
    params.onClosed         = [this] () { m_popup = nullptr; Close(); };

    hr = m_popup->Show (std::move (params));

    if (FAILED (hr))
    {
        Close();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripFlyout::Close
//
//  The source is sent a hover of row 0, for none.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripFlyout::Close()
{
    DxuiPopupHost  * popup = m_popup;



    m_popup = nullptr;

    if (popup != nullptr)
    {
        popup->Close();
    }

    if (m_isOpen && m_hoveredRow >= 0 && m_source != nullptr)
    {
        m_source->OnForkRowHovered (0);
    }

    m_isOpen     = false;
    m_isOverMark = false;
    m_hoveredRow = -1;

    m_rows.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripFlyout::HoverAt
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripFlyout::HoverAt (POINT localPx)
{
    int  row = HitTestRow (localPx.y, GetRowPx(), (int) m_rows.size());



    if (row == m_hoveredRow || m_source == nullptr)
    {
        return;
    }

    m_hoveredRow = row;

    m_source->OnForkRowHovered ((row >= 0) ? m_rows[(size_t) row].id : 0);

    if (m_popup != nullptr)
    {
        m_popup->MarkDirty();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripFlyout::ClickAt
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripFlyout::ClickAt (POINT localPx)
{
    int                       row    = HitTestRow (localPx.y, GetRowPx(), (int) m_rows.size());
    uint64_t                  id     = 0;
    IDxuiImageStripSource   * source = m_source;



    if (row < 0 || source == nullptr)
    {
        return;
    }

    id = m_rows[(size_t) row].id;

    Close();

    source->OnForkRowClicked (id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripFlyout::IsPointerOn
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiStripFlyout::IsPointerOn() const
{
    POINT  cursor = {};



    if (m_popup == nullptr || !GetCursorPos (&cursor))
    {
        return false;
    }

    return PtInRect (&m_popup->GetPlacedRectScreenPx(), cursor) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripFlyout::TryGetPlacedRect
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiStripFlyout::TryGetPlacedRect (RECT & outScreenPx) const
{
    if (!m_isOpen || m_popup == nullptr || !m_popup->IsOpen())
    {
        return false;
    }

    outScreenPx = m_popup->GetPlacedRectScreenPx();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripFlyout::HitTestRow
//
////////////////////////////////////////////////////////////////////////////////

int DxuiStripFlyout::HitTestRow (
    int  localY,
    int  rowPx,
    int  count)
{
    int  row = (rowPx > 0 && localY >= 0) ? localY / rowPx : -1;



    return (row >= 0 && row < count) ? row : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripFlyout::GetRowPx
//
//  A row is its picture with padding above and below, in the popup's
//  pixels.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiStripFlyout::GetRowPx() const
{
    float  rowDip = kPictureDip + 2.0f * kPadDip;



    if (m_popupHost == nullptr)
    {
        return (int) rowDip;
    }

    return m_popupHost->GetScaler().ToPx ((int) rowDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripFlyout::GetSizeDip
//
////////////////////////////////////////////////////////////////////////////////

SIZE DxuiStripFlyout::GetSizeDip() const
{
    float  pictureWide = kPictureDip * m_aspect;
    float  wide        = kPadDip * 3.0f + pictureWide + kTextDip;
    float  tall        = (kPictureDip + 2.0f * kPadDip) * (float) m_rows.size();



    return SIZE { (LONG) std::lround (wide), (LONG) std::lround (tall) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripFlyout::Render
//
//  Popup-local pixels: each row its picture at the left, or a dark panel
//  until it is drawn, and its two lines beside it; the hovered row lit.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripFlyout::Render (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text)
{
    constexpr uint32_t  kInkArgb   = 0xFFF0F0F0u;
    constexpr uint32_t  kDimArgb   = 0xFFB0B0B0u;
    constexpr uint32_t  kHoverArgb = 0x30FFFFFFu;
    constexpr uint32_t  kEmptyArgb = 0xFF101010u;



    const DxuiDpiScaler  & scaler  = m_popupHost->GetScaler();
    float                  pad     = scaler.ToPxf (kPadDip);
    float                  tall    = scaler.ToPxf (kPictureDip);
    float                  wide    = tall * m_aspect;
    float                  textX   = pad * 2.0f + wide;
    float                  textW   = scaler.ToPxf (kTextDip);
    float                  font    = scaler.ToPxf (kFontDip);
    float                  rowPx   = (float) GetRowPx();
    float                  top     = 0.0f;
    HRESULT                hr      = S_OK;



    UNREFERENCED_PARAMETER (painter);

    for (size_t i = 0; i < m_rows.size(); i++)
    {
        const DxuiStripForkRow  & row = m_rows[i];



        top = rowPx * (float) i;

        if ((int) i == m_hoveredRow)
        {
            hr = text.DrawIconBitmap (&kHoverArgb, 1, 1, 0.0f, top, textX + textW + pad, rowPx);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (row.picture != nullptr && row.picture->width > 0)
        {
            hr = text.DrawFramebuffer (row.picture->bgraPremul.data(), row.picture->width, row.picture->height, pad, top + pad, wide, tall);
        }
        else
        {
            hr = text.DrawIconBitmap (&kEmptyArgb, 1, 1, pad, top + pad, wide, tall);
        }

        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (row.ranFor.c_str(), textX, top + pad, textW, tall / 2.0f, kInkArgb, font, DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (row.leftAt.c_str(), textX, top + pad + tall / 2.0f, textW, tall / 2.0f, kDimArgb, font, DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}
