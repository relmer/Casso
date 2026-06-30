#include "Pch.h"

#include "DialogBodyContent.h"

#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiButton.h"


static constexpr int     s_kLineHeightDip = 20;
static constexpr int     s_kItemGapDip    = 6;
static constexpr int     s_kIconGapDip    = 12;
static constexpr size_t  s_kWrapColumns   = 52;
static constexpr int     s_kShellExecOk   = 0;     // ignored ShellExecute result reset value




////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::SetRuns
//
//  Builds one child widget per body run -- a wrapped DxuiLabel for normal
//  text, a DxuiButton(Link) for a hyperlink -- and records an estimated
//  line count per run (used for vertical sizing).
//
////////////////////////////////////////////////////////////////////////////////

void DialogBodyContent::SetRuns (const std::vector<DialogTextRun> & runs, uint32_t textArgb)
{
    m_items.clear();
    m_items.reserve (runs.size());

    for (const DialogTextRun & run : runs)
    {
        Item    item;
        int     lines   = 1;
        size_t  lineLen = 0;


        for (wchar_t ch : run.text)
        {
            if (ch == L'\n')
            {
                lines++;
                lineLen = 0;
            }
            else
            {
                lineLen++;

                if (lineLen >= s_kWrapColumns)
                {
                    lines++;
                    lineLen = 0;
                }
            }
        }

        item.lines = lines;

        if (run.isHyperlink)
        {
            DxuiButton  &  link = Add<DxuiButton>();
            std::wstring   url  = run.hyperlinkUrl;

            link.SetLabel   (run.text);
            link.SetVariant (DxuiButton::Variant::Link);
            link.SetClick   ([url] ()
                             {
                                 INT_PTR  rc = (INT_PTR) ShellExecuteW (nullptr, L"open", url.c_str(),
                                                                        nullptr, nullptr, SW_SHOWNORMAL);

                                 IGNORE_RETURN_VALUE (rc, s_kShellExecOk);
                             });
            item.widget = &link;
        }
        else
        {
            DxuiLabel  &  label = Add<DxuiLabel>();

            label.SetText      (run.text);
            label.SetColorArgb (textArgb);
            label.SetVAlign    (DxuiTextVAlign::Top);
            item.widget = &label;
        }

        m_items.push_back (item);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::SetIcon
//
////////////////////////////////////////////////////////////////////////////////

void DialogBodyContent::SetIcon (std::vector<uint32_t> bgraPremul, int srcW, int srcH, int displaySizeDip)
{
    m_iconPixels  = std::move (bgraPremul);
    m_iconSrcW    = srcW;
    m_iconSrcH    = srcH;
    m_iconSizeDip = displaySizeDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::PreferredHeightDip
//
////////////////////////////////////////////////////////////////////////////////

int DialogBodyContent::PreferredHeightDip () const
{
    int  total = 0;


    if (!m_iconPixels.empty() && m_iconSizeDip > 0)
    {
        total += m_iconSizeDip + s_kIconGapDip;
    }

    for (const Item & item : m_items)
    {
        total += item.lines * s_kLineHeightDip + s_kItemGapDip;
    }

    return total;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::Layout
//
//  Stacks the run widgets top-down within the (physical-pixel) content
//  rect, each sized to its estimated line count.
//
////////////////////////////////////////////////////////////////////////////////

void DialogBodyContent::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int  linePx = scaler.Px (s_kLineHeightDip);
    int  gapPx  = scaler.Px (s_kItemGapDip);
    int  y      = boundsPx.top;


    SetBounds (boundsPx);

    m_iconRectPx = {};

    if (!m_iconPixels.empty() && m_iconSizeDip > 0)
    {
        int  iconPx = scaler.Px (m_iconSizeDip);
        int  cx     = (boundsPx.left + boundsPx.right) / 2;

        m_iconRectPx.left   = cx - iconPx / 2;
        m_iconRectPx.top    = y;
        m_iconRectPx.right  = cx + iconPx / 2;
        m_iconRectPx.bottom = y + iconPx;
        y += iconPx + scaler.Px (s_kIconGapDip);
    }

    for (Item & item : m_items)
    {
        int   hPx = item.lines * linePx;
        RECT  b   = { boundsPx.left, y, boundsPx.right, y + hPx };

        if (item.widget != nullptr)
        {
            item.widget->Layout (b, scaler);
        }

        y += hPx + gapPx;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::Paint
//
//  Draws the optional top icon (premultiplied BGRA bitmap), then fans the
//  paint out to the run widgets via the base panel.
//
////////////////////////////////////////////////////////////////////////////////

void DialogBodyContent::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (!m_iconPixels.empty() && m_iconRectPx.right > m_iconRectPx.left)
    {
        HRESULT  hr = text.DrawIconBitmap (m_iconPixels.data(),
                                           m_iconSrcW,
                                           m_iconSrcH,
                                           (float) m_iconRectPx.left,
                                           (float) m_iconRectPx.top,
                                           (float) (m_iconRectPx.right  - m_iconRectPx.left),
                                           (float) (m_iconRectPx.bottom - m_iconRectPx.top));

        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    DxuiPanel::Paint (painter, text, theme);
}
