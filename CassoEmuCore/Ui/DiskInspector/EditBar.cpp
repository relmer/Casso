#include "Pch.h"

#include "Ui/DiskInspector/EditBar.h"
#include "Ui/DiskInspector/InspectorText.h"





static constexpr int  s_kInsetDip     = 4;
static constexpr int  s_kGapDip       = 8;
static constexpr int  s_kChecksumDip  = 196;
static constexpr int  s_kCountDip     = 190;
static constexpr int  s_kDiscardSecDip = 150;
static constexpr int  s_kUndoDip      = 144;
static constexpr int  s_kRedoDip      = 140;
static constexpr int  s_kMinNoteDip   = 120;
static constexpr int  s_kNoteCharDip  = 7;
static constexpr int  s_kPauseDip     = 230;
static constexpr int  s_kCancelDip    = 76;
static constexpr int  s_kUseBDip      = 120;





////////////////////////////////////////////////////////////////////////////////
//
//  EditBar::EditBar
//
////////////////////////////////////////////////////////////////////////////////

EditBar::EditBar()
{
    m_checksum      = CreateChild<DxuiCheckbox> (L"Recompute the checksum", true);
    m_count         = CreateChild<DxuiLabel>    (L"", DxuiTextRole::Body,  DxuiTextHAlign::Left);
    m_note          = CreateChild<DxuiLabel>    (L"", DxuiTextRole::Muted, DxuiTextHAlign::Left);
    m_discardSector = CreateChild<DxuiButton>   (L"Undo sector changes");
    m_undoApplied   = CreateChild<DxuiButton>   (L"Undo save");
    m_redoApplied   = CreateChild<DxuiButton>   (L"Redo save");
    m_pauseApply    = CreateChild<DxuiButton>   (L"Pause and save");
    m_cancelApply   = CreateChild<DxuiButton>   (L"Cancel");
    m_useB          = CreateChild<DxuiButton>   (L"Use B's bytes");

    m_checksum->SetOnChange     ([this] (bool isChecked) { if (m_onChecksum) { m_onChecksum (isChecked); } });
    m_discardSector->SetOnClick ([this] () { if (m_onDiscardSector) { m_onDiscardSector(); } });
    m_undoApplied->SetOnClick   ([this] () { if (m_onUndoApplied)   { m_onUndoApplied(); } });
    m_redoApplied->SetOnClick   ([this] () { if (m_onRedoApplied)   { m_onRedoApplied(); } });
    m_pauseApply->SetOnClick    ([this] () { if (m_onPauseApply)    { m_onPauseApply(); } });
    m_cancelApply->SetOnClick   ([this] () { if (m_onCancelApply)   { m_onCancelApply(); } });
    m_useB->SetOnClick          ([this] () { if (m_onUseB)          { m_onUseB(); } });

    SetState (EditBarState());
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditBar::SetState
//
//  "Undo sector changes" and the checksum option are visible while editing,
//  and enabled while the selected sector has an edit. Saving and discarding
//  the disk's edits are on the File and Edit menus.
//
////////////////////////////////////////////////////////////////////////////////

void EditBar::SetState (const EditBarState & state)
{
    m_checksum->SetVisible (state.isEditing && state.hasChecksum);
    m_checksum->SetChecked (state.isChecksumOn);
    m_checksum->SetEnabled (state.hasSectorEdit && !state.isChecksumLocked);

    m_count->SetText (FormatCount (state.bytes, state.sectors, m_unit.c_str(), m_units.c_str()));
    m_note->SetText  (state.note);

    m_discardSector->SetVisible (state.isEditing);
    m_discardSector->SetEnabled (state.hasSectorEdit);
    //  While a save waits, only the ways to end the wait are offered, so
    //  the pending edits cannot change under it (FR-103).
    m_undoApplied->SetVisible   (state.canUndoApplied && !state.isWaiting);
    m_redoApplied->SetVisible   (state.canRedoApplied && !state.isWaiting);
    m_pauseApply->SetVisible    (state.isWaiting);
    m_pauseApply->SetLabel      (state.isMachinePaused ? L"Run to a safe point and save" : L"Pause and save");
    m_cancelApply->SetVisible   (state.isWaiting);
    m_useB->SetVisible          (state.canUseB && !state.isWaiting);

    //  Each control's place depends on which controls are visible.
    if (m_hasLayout)
    {
        Layout (GetBounds(), m_layoutScaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditBar::GetTip
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EditBar::GetTip (POINT pointPx, RECT & outAnchorPx) const
{
    struct ControlTip
    {
        const IDxuiControl  * control;
        std::wstring          text;
        std::wstring          whyUnavailable;
    };

    const ControlTip  tips[] =
    {
        { m_checksum,      L"Write a new data checksum for the sector's new bytes",
                           L"A sector image (.dsk, .do or .po) stores only sector data, so its checksum is always recomputed" },
        { m_discardSector, L"Drop the selected " + m_unit + L"'s pending edits",                               L"" },
        { m_pauseApply,    L"Let the machine run to the next moment the head is clear of the edited fields, and save there", L"" },
        { m_cancelApply,   L"Stop waiting; the edits stay pending",                                         L"" },
        { m_useB,          L"Put B's bytes for the paired " + m_unit + L" into this " + m_unit + L" as a pending edit", L"" },
    };

    std::wstring  tip;
    RECT          bounds = {};



    for (const ControlTip & entry : tips)
    {
        bounds = entry.control->GetBounds();

        if (tip.empty() && entry.control->IsVisible() && PtInRect (&bounds, pointPx))
        {
            tip         = (!entry.control->IsEnabled() && !entry.whyUnavailable.empty()) ? entry.whyUnavailable : entry.text;
            outAnchorPx = entry.control->GetBounds();
        }
    }

    return tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditBar::FormatCount
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EditBar::FormatCount (int bytes, int units, LPCWSTR unit, LPCWSTR unitPlural)
{
    return (units > 0) ? InspectorText::FormatCount (bytes, L"byte", L"bytes") + L" in " + InspectorText::FormatCount (units, unit, unitPlural) + L" changed"
                       : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditBar::SetUnit
//
////////////////////////////////////////////////////////////////////////////////

void EditBar::SetUnit (const std::wstring & unit, const std::wstring & unitPlural)
{
    m_unit  = unit;
    m_units = unitPlural;
    m_discardSector->SetLabel (L"Undo " + unit + L" changes");
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditBar::Layout
//
//  The selected sector's controls from the left, then the count and the
//  note, and the disk's controls from the right, on a second row when the
//  first is too full. A note that does not fit beside the controls is
//  placed below them, across the whole width.
//
////////////////////////////////////////////////////////////////////////////////

void EditBar::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int   inset    = scaler.ToPx (s_kInsetDip);
    int   gap      = scaler.ToPx (s_kGapDip);
    int   rowH     = scaler.ToPx (kRowDip);
    int   width    = boundsPx.right - boundsPx.left;
    int   controls = HasControls (scaler) ? GetControlRowCount (width, scaler) : 0;
    int   noteRows = GetNoteRowCount (width, scaler);
    bool  isTwo    = controls > 1;
    int   top      = boundsPx.top + inset;
    int   bottom   = boundsPx.top + rowH - inset;
    int   x        = boundsPx.left;
    int   r        = boundsPx.right;
    int   rTop     = isTwo ? top + rowH : top;
    int   rBot     = isTwo ? bottom + rowH : bottom;



    SetBounds (boundsPx);
    m_layoutScaler = scaler;
    m_hasLayout    = true;

    auto  placeLeft  = [&] (IDxuiControl * control, int widthDip) { if (control->IsVisible()) { control->Layout ({ x, top, x + scaler.ToPx (widthDip), bottom }, scaler); x += scaler.ToPx (widthDip) + gap; } };
    auto  placeRight = [&] (IDxuiControl * control, int widthDip) { if (control->IsVisible()) { control->Layout ({ r - scaler.ToPx (widthDip), rTop, r, rBot }, scaler); r -= scaler.ToPx (widthDip) + gap; } };

    placeLeft  (m_discardSector, s_kDiscardSecDip);
    placeLeft  (m_checksum,      s_kChecksumDip);
    placeLeft  (m_useB,          s_kUseBDip);

    placeRight (m_cancelApply,   s_kCancelDip);
    placeRight (m_pauseApply,    s_kPauseDip);
    placeRight (m_redoApplied,   s_kRedoDip);
    placeRight (m_undoApplied,   s_kUndoDip);

    r = isTwo ? boundsPx.right : r;

    m_count->Layout ({ x, top, std::max (x, std::min (r, x + scaler.ToPx (s_kCountDip))), bottom }, scaler);
    x = std::min (r, x + (m_count->GetText().empty() ? 0 : scaler.ToPx (s_kCountDip) + gap));

    if (noteRows > 0)
    {
        m_note->Layout ({ boundsPx.left, boundsPx.top + controls * rowH + inset, boundsPx.right, boundsPx.top + (controls + noteRows) * rowH - inset }, scaler);
    }
    else
    {
        m_note->Layout ({ x, top, std::max (x, r), bottom }, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditBar::GetHeightPx
//
//  At least one row, whether or not anything is in it, so the bytes under
//  the bar don't move when editing starts or stops.
//
////////////////////////////////////////////////////////////////////////////////

int EditBar::GetHeightPx (int widthPx, const DxuiDpiScaler & scaler) const
{
    return GetRowCount (widthPx, scaler) * scaler.ToPx (kRowDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditBar::GetRowCount
//
////////////////////////////////////////////////////////////////////////////////

int EditBar::GetRowCount (int widthPx, const DxuiDpiScaler & scaler) const
{
    int  controls = HasControls (scaler) ? GetControlRowCount (widthPx, scaler) : 0;



    return std::max (1, controls + GetNoteRowCount (widthPx, scaler));
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditBar::GetControlRowCount
//
//  One row when every visible control fits the width, else two, with the
//  disk's controls on the second.
//
////////////////////////////////////////////////////////////////////////////////

int EditBar::GetControlRowCount (int widthPx, const DxuiDpiScaler & scaler) const
{
    int  right  = GetGroupWidthPx (false, scaler);
    int  total  = GetGroupWidthPx (true, scaler) + right + scaler.ToPx (m_count->GetText().empty() ? s_kMinNoteDip : s_kCountDip);



    return (right > 0 && total > widthPx) ? 2 : 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditBar::GetNoteRowCount
//
//  None when the note fits on the first row beside the controls; otherwise
//  the rows its text takes across the whole width, from an average
//  character width.
//
////////////////////////////////////////////////////////////////////////////////

int EditBar::GetNoteRowCount (int widthPx, const DxuiDpiScaler & scaler) const
{
    int  controls = GetControlRowCount (widthPx, scaler);
    int  left     = GetGroupWidthPx (true, scaler) + (m_count->GetText().empty() ? 0 : scaler.ToPx (s_kCountDip + s_kGapDip));
    int  right    = (controls == 1) ? GetGroupWidthPx (false, scaler) : 0;
    int  noteW    = scaler.ToPx (static_cast<int> (m_note->GetText().size()) * s_kNoteCharDip);
    int  usable   = std::max (1, widthPx);



    if (m_note->GetText().empty() || noteW <= widthPx - left - right)
    {
        return 0;
    }

    return (noteW + usable - 1) / usable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditBar::HasControls
//
//  Whether any control or the count is visible, so the controls take a row.
//
////////////////////////////////////////////////////////////////////////////////

bool EditBar::HasControls (const DxuiDpiScaler & scaler) const
{
    return GetGroupWidthPx (true, scaler) + GetGroupWidthPx (false, scaler) > 0 || !m_count->GetText().empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditBar::GetGroupWidthPx
//
//  The width of the visible controls of the selected sector, or of the
//  disk, with the gaps after them.
//
////////////////////////////////////////////////////////////////////////////////

int EditBar::GetGroupWidthPx (bool isLeft, const DxuiDpiScaler & scaler) const
{
    const std::array<std::pair<const IDxuiControl *, int>, 3>  left  = {{ { m_discardSector, s_kDiscardSecDip }, { m_checksum, s_kChecksumDip }, { m_useB, s_kUseBDip } }};
    const std::array<std::pair<const IDxuiControl *, int>, 4>  right = {{ { m_undoApplied, s_kUndoDip }, { m_redoApplied, s_kRedoDip },
                                                                          { m_pauseApply, s_kPauseDip }, { m_cancelApply, s_kCancelDip } }};
    int                                                        width = 0;



    for (const auto & [control, widthDip] : isLeft ? std::span<const std::pair<const IDxuiControl *, int>> (left) : std::span<const std::pair<const IDxuiControl *, int>> (right))
    {
        width += control->IsVisible() ? scaler.ToPx (widthDip + s_kGapDip) : 0;
    }

    return width;
}
