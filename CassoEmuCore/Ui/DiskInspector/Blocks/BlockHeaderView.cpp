#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockHeaderView.h"
#include "Ui/DiskInspector/Blocks/BlockJobQueue.h"
#include "Ui/DiskInspector/EditBar.h"
#include "Core/NumberFormat.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderView::Create
//
////////////////////////////////////////////////////////////////////////////////

void BlockHeaderView::Create (DxuiPanel & parent, StepFn step, ActionFn copy, ActionFn cancel)
{
    m_previous = parent.CreateChild<DxuiIconButton>();
    m_next     = parent.CreateChild<DxuiIconButton>();
    m_copy     = parent.CreateChild<DxuiButton> (L"Copy block");
    m_editBar  = parent.CreateChild<EditBar>();
    m_cancel   = parent.CreateChild<DxuiButton> (L"Cancel");

    m_previous->SetGlyph          (s_kpszGlyphPrev);
    m_previous->SetAccessibleName (L"Previous block");
    m_previous->SetOnClick        ([step] () { step (-1); });
    m_next->SetGlyph              (s_kpszGlyphNext);
    m_next->SetAccessibleName     (L"Next block");
    m_next->SetOnClick            ([step] () { step (1); });

    m_copy->SetOnClick   (std::move (copy));
    m_cancel->SetOnClick (std::move (cancel));
    m_editBar->SetUnit   (L"block", L"blocks");

    m_cancel->SetVisible (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderView::Layout
//
//  The title row with Copy block at its right end, the field line, then the
//  edit bar or a long job's progress row.
//
////////////////////////////////////////////////////////////////////////////////

int BlockHeaderView::Layout (const RECT & column, int top, bool isLongJob, const DxuiDpiScaler & scaler)
{
    int   pad    = scaler.ToPx ((int) kPadDip);
    int   row    = scaler.ToPx ((int) kRowDip);
    int   line   = scaler.ToPx ((int) kLineDip);
    int   button = scaler.ToPx ((int) kButtonDip);
    int   y      = top;
    int   barH   = isLongJob ? row : m_editBar->GetHeightPx ((int) (column.right - column.left), scaler);
    bool  isRow  = barH > 0;



    for (IDxuiControl * control : { (IDxuiControl *) m_previous, (IDxuiControl *) m_next, (IDxuiControl *) m_copy })
    {
        control->SetVisible (true);
    }

    m_editBar->SetVisible (!isLongJob && barH > 0);

    m_previous->Layout ({ column.left, y, column.left + row, y + row }, scaler);
    m_next->Layout     ({ column.left + row, y, column.left + 2 * row, y + row }, scaler);
    m_titleRect = { column.left + 2 * row + pad, y, column.right - button - pad, y + row };
    m_copy->Layout ({ column.right - button, y, column.right, y + row }, scaler);
    y += row + pad;

    m_fieldRect = { column.left, y, column.right, y + line };
    y += line + pad;

    m_rowRect = { column.left, y, column.right, y + barH };

    m_editBar->Layout ({ column.left, y, column.right, y + barH }, scaler);
    m_cancel->Layout  ({ column.right - button, y, column.right, y + row }, scaler);

    return m_rowRect.bottom + (isRow ? pad : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderView::ShowBlock
//
////////////////////////////////////////////////////////////////////////////////

void BlockHeaderView::ShowBlock (uint32_t block, vector<BlockCapsule> capsules)
{
    m_title    = L"Block " + NumberFormat::FormatInteger (block);
    m_capsules = std::move (capsules);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderView::SetSteps
//
////////////////////////////////////////////////////////////////////////////////

void BlockHeaderView::SetSteps (bool hasPrevious, bool hasNext)
{
    m_previous->SetEnabled (hasPrevious);
    m_next->SetEnabled     (hasNext);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderView::SetBusy
//
////////////////////////////////////////////////////////////////////////////////

void BlockHeaderView::SetBusy (bool isLongJob, bool canCancel)
{
    m_cancel->SetVisible  (isLongJob && canCancel);
    m_copy->SetEnabled    (!isLongJob);
    m_editBar->SetEnabled (!isLongJob);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderView::SetEditState
//
////////////////////////////////////////////////////////////////////////////////

bool BlockHeaderView::SetEditState (const EditBarState & state, const DxuiDpiScaler & scaler)
{
    int  width  = m_rowRect.right - m_rowRect.left;
    int  before = width > 0 ? m_editBar->GetHeightPx (width, scaler) : 0;



    m_editBar->SetState (state);

    return width > 0 && scaler.GetDpi() != 0 && m_editBar->GetHeightPx (width, scaler) != before;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void BlockHeaderView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiDpiScaler & scaler, const BlockJobQueue & jobs, bool isOpen) const
{
    if (isOpen && !m_title.empty())
    {
        PaintTitle (painter, text, theme, scaler);
    }

    if (jobs.IsLongJob())
    {
        PaintProgress (painter, text, theme, scaler, jobs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderView::PaintTitle
//
//  "Block 2" and its capsules after it, in the status band's chip style:
//  Free or Used, the role, and the files. Capsules that don't fit are left
//  out. Then the field line.
//
////////////////////////////////////////////////////////////////////////////////

void BlockHeaderView::PaintTitle (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiDpiScaler & scaler) const
{
    constexpr float     kChipPadDip    = 8.0f;
    constexpr float     kChipHeightDip = 20.0f;
    constexpr uint32_t  kBadColor      = 0xFFE03C3C;



    float    font   = scaler.ToPxf (kFontDip);
    float    title  = scaler.ToPxf (kTitleDip);
    float    chipH  = scaler.ToPxf (kChipHeightDip);
    float    chipP  = scaler.ToPxf (kChipPadDip);
    float    gap    = scaler.ToPxf (kPadDip);
    float    top    = (float) m_titleRect.top;
    float    height = (float) (m_titleRect.bottom - m_titleRect.top);
    float    chipY  = top + (height - chipH) / 2.0f;
    float    x      = (float) m_titleRect.left;
    float    w      = 0.0f;
    float    h      = 0.0f;
    HRESULT  hr     = S_OK;



    hr = text.MeasureStringWeighted (m_title.c_str(), title, DxuiTheme::kBodyFace, DxuiFontWeight::SemiBold, w, h);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawString (m_title.c_str(), x, top, w + 1.0f, height, theme.Foreground(), title, DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    x += w + gap;

    for (const BlockCapsule & capsule : m_capsules)
    {
        text.MeasureString (capsule.text.c_str(), font, DxuiTheme::kBodyFace, w, h);
        w += 2 * chipP;

        if (x + w > m_titleRect.right)
        {
            break;
        }

        painter.FillRoundedRect    (x, chipY, w, chipH, chipH / 2, theme.ButtonIdle());
        painter.OutlineRoundedRect (x, chipY, w, chipH, chipH / 2, scaler.ToPxf (1.0f), capsule.isBad ? kBadColor : theme.ButtonBorder());

        hr = text.DrawString (capsule.text.c_str(), x, chipY, w, chipH, capsule.isBad ? kBadColor : theme.Foreground(), font, DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        x += w + gap / 2;
    }

    hr = text.DrawString (m_fieldLine.c_str(), (float) m_fieldRect.left, (float) m_fieldRect.top, (float) (m_fieldRect.right - m_fieldRect.left),
                          (float) (m_fieldRect.bottom - m_fieldRect.top), theme.Foreground(), font, DxuiTheme::kBodyFace, DxuiTextHAlign::Left,
                          DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderView::PaintProgress
//
//  A long job's bar and label in the row above the hex view. A job with no
//  count yet has an empty bar.
//
////////////////////////////////////////////////////////////////////////////////

void BlockHeaderView::PaintProgress (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiDpiScaler & scaler, const BlockJobQueue & jobs) const
{
    constexpr float  kBarHighDip = 8.0f;



    float         left   = (float) m_rowRect.left;
    float         top    = (float) m_rowRect.top;
    float         height = (float) (m_rowRect.bottom - m_rowRect.top);
    float         barW   = scaler.ToPxf (kBarDip);
    float         barH   = scaler.ToPxf (kBarHighDip);
    float         barTop = top + (height - barH) / 2.0f;
    double        share  = jobs.GetShare();
    std::wstring  label  = jobs.BuildProgressLabel();
    float         textX  = left + barW + scaler.ToPxf (kPadDip);
    float         textW  = (float) m_rowRect.right - textX - scaler.ToPxf (kButtonDip + kPadDip);
    HRESULT       hr     = S_OK;



    painter.FillRoundedRect    (left, barTop, barW, barH, barH / 2, theme.ButtonIdle());
    painter.FillRoundedRect    (left, barTop, (float) (barW * share), barH, barH / 2, theme.Accent());
    painter.OutlineRoundedRect (left, barTop, barW, barH, barH / 2, 1.0f, theme.ButtonBorder());

    hr = text.DrawString (label.c_str(), textX, top, std::max (0.0f, textW), height, theme.Foreground(), scaler.ToPxf (kFontDip), s_kpszFont,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
