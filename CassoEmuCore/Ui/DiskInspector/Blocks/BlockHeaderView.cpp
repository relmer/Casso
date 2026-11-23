#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockHeaderView.h"
#include "Ui/DiskInspector/Blocks/BlockJobQueue.h"
#include "Ui/DiskInspector/InspectorText.h"
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
    m_edit     = parent.CreateChild<DxuiButton> (L"Edit");
    m_copy     = parent.CreateChild<DxuiButton> (L"Copy block");
    m_undo     = parent.CreateChild<DxuiButton> (L"Undo block changes");
    m_useOther = parent.CreateChild<DxuiButton> (L"Use B's bytes");
    m_cancel   = parent.CreateChild<DxuiButton> (L"Cancel");

    m_previous->SetGlyph          (s_kpszGlyphPrev);
    m_previous->SetAccessibleName (L"Previous block");
    m_previous->SetOnClick        ([step] () { step (-1); });
    m_next->SetGlyph              (s_kpszGlyphNext);
    m_next->SetAccessibleName     (L"Next block");
    m_next->SetOnClick            ([step] () { step (1); });

    m_edit->SetOnClick     ([this] () { if (m_onEdit) { m_onEdit(); } });
    m_undo->SetOnClick     ([this] () { if (m_onUndo) { m_onUndo(); } });
    m_useOther->SetOnClick ([this] () { if (m_onUseOther) { m_onUseOther(); } });
    m_copy->SetOnClick     (std::move (copy));
    m_cancel->SetOnClick   (std::move (cancel));

    m_undo->SetVisible     (false);
    m_useOther->SetVisible (false);
    m_cancel->SetVisible   (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderView::Layout
//
//  The title row with Edit and Copy block at its right end, then the second
//  row: Undo block changes and Use B's bytes while editing, then the count,
//  the field line and the note; or a long job's progress and Cancel.
//
////////////////////////////////////////////////////////////////////////////////

int BlockHeaderView::Layout (const RECT & column, int top, bool isLongJob, const DxuiDpiScaler & scaler)
{
    int   pad    = scaler.ToPx ((int) kPadDip);
    int   row    = scaler.ToPx ((int) kRowDip);
    int   button = scaler.ToPx ((int) kButtonDip);
    int   edit   = scaler.ToPx ((int) kEditDip);
    int   undo   = scaler.ToPx ((int) kUndoDip);
    int   y      = top;
    int   x      = column.left;
    bool  isUndo = !isLongJob && m_state.isEditing;
    bool  isUseB = !isLongJob && m_state.canUseOther;



    m_isLongJob = isLongJob;

    m_previous->Layout ({ column.left, y, column.left + row, y + row }, scaler);
    m_next->Layout     ({ column.left + row, y, column.left + 2 * row, y + row }, scaler);
    m_titleRect = { column.left + 2 * row + pad, y, column.right - button - edit - 2 * pad, y + row };
    m_edit->Layout ({ column.right - button - pad - edit, y, column.right - button - pad, y + row }, scaler);
    m_copy->Layout ({ column.right - button, y, column.right, y + row }, scaler);
    y += row + pad;

    m_rowRect = { column.left, y, column.right, y + row };

    m_undo->SetVisible     (isUndo);
    m_useOther->SetVisible (isUseB);

    if (isUndo)
    {
        m_undo->Layout ({ x, y, x + undo, y + row }, scaler);
        x += undo + pad;
    }

    if (isUseB)
    {
        m_useOther->Layout ({ x, y, x + button, y + row }, scaler);
        x += button + pad;
    }

    m_infoRect = { x, y, isLongJob ? x : (int) column.right, y + row };
    m_cancel->Layout ({ column.right - button, y, column.right, y + row }, scaler);

    return m_rowRect.bottom + pad;
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
    m_cancel->SetVisible (isLongJob && canCancel);
    m_copy->SetEnabled   (!isLongJob);
    m_edit->SetEnabled   (!isLongJob && m_state.canEdit);
    m_undo->SetEnabled   (!isLongJob && m_state.canUndo);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderView::SetEditState
//
//  Edit reads Done, and is drawn as the primary button, while editing.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockHeaderView::SetEditState (const BlockEditState & state)
{
    bool  isMoved = state.isEditing != m_state.isEditing || state.canUseOther != m_state.canUseOther;



    if (state == m_state)
    {
        return false;
    }

    m_state = state;

    m_edit->SetLabel       (state.isEditing ? L"Done" : L"Edit");
    m_edit->SetVariant     (state.isEditing ? DxuiButton::Variant::Primary : DxuiButton::Variant::Default);
    m_edit->SetEnabled     (!m_isLongJob && (state.canEdit || state.isEditing));
    m_undo->SetEnabled     (!m_isLongJob && state.canUndo);
    m_useOther->SetEnabled (!m_isLongJob && state.canUseOther);

    return isMoved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderView::FormatBlockCount
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockHeaderView::FormatBlockCount (int bytes)
{
    return bytes > 0 ? InspectorText::FormatCount (bytes, L"byte", L"bytes") + L" changed in this block" : std::wstring();
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
    else if (isOpen)
    {
        PaintInfo (text, theme, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderView::PaintTitle
//
//  "Block 2" and its capsules after it, in the status band's chip style:
//  Free or Used, the role, and the files. Capsules that don't fit are left
//  out.
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderView::PaintInfo
//
//  The second row's text after its buttons: the bytes changed in the block
//  while editing, the field line, and the last save's result, muted.
//
////////////////////////////////////////////////////////////////////////////////

void BlockHeaderView::PaintInfo (IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiDpiScaler & scaler) const
{
    std::wstring  line;
    HRESULT       hr   = S_OK;



    for (const std::wstring & part : { m_state.isEditing ? FormatBlockCount (m_state.blockBytes) : std::wstring(), m_fieldLine, m_state.note })
    {
        line += (line.empty() || part.empty()) ? part : L"    " + part;
    }

    hr = text.DrawString (line.c_str(), (float) m_infoRect.left, (float) m_infoRect.top, (float) (m_infoRect.right - m_infoRect.left),
                          (float) (m_infoRect.bottom - m_infoRect.top), theme.Foreground(), scaler.ToPxf (kFontDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left,
                          DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderView::PaintProgress
//
//  A long job's bar and label in the second row. A job with no count yet
//  has an empty bar.
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
