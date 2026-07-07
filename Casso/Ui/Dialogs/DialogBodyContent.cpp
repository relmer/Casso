#include "Pch.h"

#include "DialogBodyContent.h"

#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiButton.h"


static constexpr int      s_kLineHeightDip = 20;
static constexpr int      s_kItemGapDip    = 6;
static constexpr int      s_kIconGapDip    = 12;
static constexpr int      s_kGlyphGapDip   = 12;
static constexpr size_t   s_kWrapColumns   = 52;
static constexpr int      s_kShellExecOk   = 0;     // ignored ShellExecute result reset value
static constexpr wchar_t  s_kMdl2Family[]  = L"Segoe MDL2 Assets";




////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::SetRuns
//
//  Builds one child widget per body run -- a wrapped DxuiLabel for normal
//  text, a DxuiButton(Link) for a hyperlink -- and records an estimated
//  line count per run (used for vertical sizing).
//
////////////////////////////////////////////////////////////////////////////////

void DialogBodyContent::SetRuns (const std::vector<DialogTextRun> & runs)
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
            link.SetOnClick   ([url] ()
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
            label.SetTextRole  (DxuiTextRole::Body);
            label.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);
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
//  DialogBodyContent::SetGlyphIcon
//
////////////////////////////////////////////////////////////////////////////////

void DialogBodyContent::SetGlyphIcon (wchar_t glyph, uint32_t argb, int sizeDip)
{
    m_glyph        = glyph;
    m_glyphArgb    = argb;
    m_glyphSizeDip = sizeDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent::PreferredHeightDip
//
////////////////////////////////////////////////////////////////////////////////

int DialogBodyContent::PreferredHeightDip () const
{
    int  iconTop = 0;
    int  runs    = 0;


    if (!m_iconPixels.empty() && m_iconSizeDip > 0)
    {
        iconTop = m_iconSizeDip + s_kIconGapDip;
    }

    for (const Item & item : m_items)
    {
        runs += item.lines * s_kLineHeightDip + s_kItemGapDip;
    }

    if (m_glyph != 0 && m_glyphSizeDip > runs)
    {
        runs = m_glyphSizeDip;
    }

    return iconTop + runs;
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
    int  linePx   = scaler.Px (s_kLineHeightDip);
    int  gapPx    = scaler.Px (s_kItemGapDip);
    int  y        = boundsPx.top;
    int  runsLeft = boundsPx.left;


    SetBounds (boundsPx);

    m_iconRectPx  = {};
    m_glyphRectPx = {};

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

    if (m_glyph != 0 && m_glyphSizeDip > 0)
    {
        int  glyphPx = scaler.Px (m_glyphSizeDip);

        m_glyphRectPx.left   = boundsPx.left;
        m_glyphRectPx.top    = y;
        m_glyphRectPx.right  = boundsPx.left + glyphPx;
        m_glyphRectPx.bottom = y + glyphPx;
        runsLeft = boundsPx.left + glyphPx + scaler.Px (s_kGlyphGapDip);
    }

    for (Item & item : m_items)
    {
        int   hPx = item.lines * linePx;
        RECT  b   = { runsLeft, y, boundsPx.right, y + hPx };

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

    if (m_glyph != 0 && m_glyphRectPx.right > m_glyphRectPx.left)
    {
        wchar_t  glyphStr[2] = { m_glyph, L'\0' };
        float    sizePx      = (float) (m_glyphRectPx.bottom - m_glyphRectPx.top);
        HRESULT  hr          = text.DrawString (glyphStr,
                                                (float) m_glyphRectPx.left,
                                                (float) m_glyphRectPx.top,
                                                (float) (m_glyphRectPx.right  - m_glyphRectPx.left),
                                                (float) (m_glyphRectPx.bottom - m_glyphRectPx.top),
                                                m_glyphArgb,
                                                sizePx,
                                                s_kMdl2Family,
                                                DxuiTextHAlign::Center,
                                                DxuiTextVAlign::Center,
                                                DxuiFontWeight::Normal);

        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    DxuiPanel::Paint (painter, text, theme);
}
