#include "Pch.h"

#include "DialogBodyContent.h"

#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiButton.h"


static constexpr int     s_kLineHeightDip = 20;
static constexpr int     s_kItemGapDip    = 6;
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
//  DialogBodyContent::PreferredHeightDip
//
////////////////////////////////////////////////////////////////////////////////

int DialogBodyContent::PreferredHeightDip () const
{
    int  total = 0;


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
