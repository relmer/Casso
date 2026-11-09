#include "Pch.h"

#include "Ui/DiskInspector/EditBar.h"
#include "Ui/DiskInspector/InspectorText.h"





static constexpr int  s_kInsetDip     = 4;
static constexpr int  s_kGapDip       = 8;
static constexpr int  s_kEditDip      = 104;
static constexpr int  s_kChecksumDip  = 196;
static constexpr int  s_kCountDip     = 190;
static constexpr int  s_kDiscardSecDip = 116;
static constexpr int  s_kDiscardDip   = 76;
static constexpr int  s_kApplyDip     = 76;
static constexpr int  s_kSaveCopyDip  = 140;
static constexpr int  s_kUndoDip      = 144;
static constexpr int  s_kRedoDip      = 140;
static constexpr int  s_kMinNoteDip   = 120;
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
    m_edit          = CreateChild<DxuiButton>   (L"Stop editing");
    m_checksum      = CreateChild<DxuiCheckbox> (L"Recompute the checksum", true);
    m_count         = CreateChild<DxuiLabel>    (L"", DxuiTextRole::Body,  DxuiTextHAlign::Left);
    m_note          = CreateChild<DxuiLabel>    (L"", DxuiTextRole::Muted, DxuiTextHAlign::Left);
    m_discardSector = CreateChild<DxuiButton>   (L"Discard sector");
    m_discard       = CreateChild<DxuiButton>   (L"Discard");
    m_apply         = CreateChild<DxuiButton>   (L"Apply...");
    m_saveCopy      = CreateChild<DxuiButton>   (L"Save edited copy...");
    m_undoApplied   = CreateChild<DxuiButton>   (L"Undo applied edit");
    m_redoApplied   = CreateChild<DxuiButton>   (L"Redo applied edit");
    m_pauseApply    = CreateChild<DxuiButton>   (L"Pause and apply");
    m_cancelApply   = CreateChild<DxuiButton>   (L"Cancel");
    m_useB          = CreateChild<DxuiButton>   (L"Use B's bytes");

    m_edit->SetOnClick          ([this] () { if (m_onEdit)          { m_onEdit(); } });
    m_checksum->SetOnChange     ([this] (bool isChecked) { if (m_onChecksum) { m_onChecksum (isChecked); } });
    m_discardSector->SetOnClick ([this] () { if (m_onDiscardSector) { m_onDiscardSector(); } });
    m_discard->SetOnClick       ([this] () { if (m_onDiscard)       { m_onDiscard(); } });
    m_apply->SetOnClick         ([this] () { if (m_onApply)         { m_onApply(); } });
    m_saveCopy->SetOnClick      ([this] () { if (m_onSaveCopy)      { m_onSaveCopy(); } });
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
//  The checksum option and "Discard sector" are for the selected sector's
//  edit, so they are visible only while it has one; the other controls for
//  the disk's edits are visible while any are pending.
//
////////////////////////////////////////////////////////////////////////////////

void EditBar::SetState (const EditBarState & state)
{
    bool  hasEdits = state.sectors > 0;



    //  Edit > "Edit sector" or F2 starts editing (FR-057); while editing is
    //  on, a click on this button ends it.
    m_edit->SetVisible (state.isEditing);

    m_checksum->SetVisible (state.hasSectorEdit);
    m_checksum->SetChecked (state.isChecksumOn);
    m_checksum->SetEnabled (!state.isChecksumLocked);

    m_applyBlocked = state.applyBlocked;
    m_count->SetText (FormatCount (state.bytes, state.sectors));
    m_note->SetText  (state.note);

    m_discardSector->SetVisible (state.hasSectorEdit);
    //  While an apply waits, only the ways to end the wait are offered, so
    //  the pending edits cannot change under it (FR-103).
    m_discard->SetVisible       (hasEdits && !state.isWaiting);
    m_apply->SetVisible         (hasEdits && !state.isWaiting);
    m_apply->SetEnabled         (state.canApply);
    m_saveCopy->SetVisible      (hasEdits && !state.isWaiting);
    m_undoApplied->SetVisible   (state.canUndoApplied && !state.isWaiting);
    m_redoApplied->SetVisible   (state.canRedoApplied && !state.isWaiting);
    m_pauseApply->SetVisible    (state.isWaiting);
    m_pauseApply->SetLabel      (state.isMachinePaused ? L"Run to a safe point and apply" : L"Pause and apply");
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
        { m_edit,          L"Stop editing the sector's bytes (Escape)",                                     L"" },
        { m_checksum,      L"Write a new data checksum for the sector's new bytes",
                           L"A sector image (.dsk, .do or .po) stores only sector data, so its checksum is always recomputed" },
        { m_discardSector, L"Drop the selected sector's pending edits",                                   L"" },
        { m_discard,       L"Drop every pending edit (Ctrl+Z brings them back)",                            L"" },
        { m_apply,         L"Write the pending edits to the disk and save it",                              m_applyBlocked },
        { m_saveCopy,      L"Save the disk with the pending edits to a new file",                           L"" },
        { m_undoApplied,   L"Put back the sectors the last apply changed, and save",                        L"" },
        { m_redoApplied,   L"Apply the last undone edit again, and save",                                   L"" },
        { m_pauseApply,    L"Let the machine run to the next moment the head is clear of the edited fields, and apply there", L"" },
        { m_cancelApply,   L"Stop waiting; the edits stay pending",                                         L"" },
        { m_useB,          L"Put B's bytes for the paired sector into this sector as a pending edit",       L"" },
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

std::wstring EditBar::FormatCount (int bytes, int sectors)
{
    return (sectors > 0) ? InspectorText::FormatCount (bytes, L"byte", L"bytes") + L" in " + InspectorText::FormatCount (sectors, L"sector", L"sectors") + L" changed"
                         : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditBar::Layout
//
//  The selected sector's controls from the left, then the count and the
//  note, and the disk's controls from the right, on a second row when the
//  first is too full.
//
////////////////////////////////////////////////////////////////////////////////

void EditBar::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int   inset  = scaler.ToPx (s_kInsetDip);
    int   gap    = scaler.ToPx (s_kGapDip);
    int   rowH   = scaler.ToPx (kRowDip);
    bool  isTwo  = GetRowCount (boundsPx.right - boundsPx.left, scaler) > 1;
    int   top    = boundsPx.top + inset;
    int   bottom = boundsPx.top + rowH - inset;
    int   x      = boundsPx.left;
    int   r      = boundsPx.right;
    int   rTop   = isTwo ? top + rowH : top;
    int   rBot   = isTwo ? bottom + rowH : bottom;



    SetBounds (boundsPx);
    m_layoutScaler = scaler;
    m_hasLayout    = true;

    auto  placeLeft  = [&] (IDxuiControl * control, int widthDip) { if (control->IsVisible()) { control->Layout ({ x, top, x + scaler.ToPx (widthDip), bottom }, scaler); x += scaler.ToPx (widthDip) + gap; } };
    auto  placeRight = [&] (IDxuiControl * control, int widthDip) { if (control->IsVisible()) { control->Layout ({ r - scaler.ToPx (widthDip), rTop, r, rBot }, scaler); r -= scaler.ToPx (widthDip) + gap; } };

    placeLeft  (m_edit,          s_kEditDip);
    placeLeft  (m_checksum,      s_kChecksumDip);
    placeLeft  (m_discardSector, s_kDiscardSecDip);
    placeLeft  (m_useB,          s_kUseBDip);

    placeRight (m_cancelApply,   s_kCancelDip);
    placeRight (m_pauseApply,    s_kPauseDip);
    placeRight (m_redoApplied,   s_kRedoDip);
    placeRight (m_undoApplied,   s_kUndoDip);
    placeRight (m_saveCopy,      s_kSaveCopyDip);
    placeRight (m_apply,         s_kApplyDip);
    placeRight (m_discard,       s_kDiscardDip);

    r = isTwo ? boundsPx.right : r;

    m_count->Layout ({ x, top, std::max (x, std::min (r, x + scaler.ToPx (s_kCountDip))), bottom }, scaler);
    x = std::min (r, x + (m_count->GetText().empty() ? 0 : scaler.ToPx (s_kCountDip) + gap));
    m_note->Layout  ({ x, top, std::max (x, r), bottom }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditBar::GetHeightPx
//
//  No height while the bar has nothing to show: no editing, no pending
//  edits and no note.
//
////////////////////////////////////////////////////////////////////////////////

int EditBar::GetHeightPx (int widthPx, const DxuiDpiScaler & scaler) const
{
    bool  isEmpty = GetGroupWidthPx (true, scaler) + GetGroupWidthPx (false, scaler) == 0 && m_count->GetText().empty() && m_note->GetText().empty();



    return isEmpty ? 0 : GetRowCount (widthPx, scaler) * scaler.ToPx (kRowDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditBar::GetRowCount
//
////////////////////////////////////////////////////////////////////////////////

int EditBar::GetRowCount (int widthPx, const DxuiDpiScaler & scaler) const
{
    int  right  = GetGroupWidthPx (false, scaler);
    int  total  = GetGroupWidthPx (true, scaler) + right + scaler.ToPx (m_count->GetText().empty() ? s_kMinNoteDip : s_kCountDip);



    return (right > 0 && total > widthPx) ? 2 : 1;
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
    const std::array<std::pair<const IDxuiControl *, int>, 4>  left  = {{ { m_edit, s_kEditDip }, { m_checksum, s_kChecksumDip }, { m_discardSector, s_kDiscardSecDip },
                                                                          { m_useB, s_kUseBDip } }};
    const std::array<std::pair<const IDxuiControl *, int>, 7>  right = {{ { m_discard, s_kDiscardDip }, { m_apply, s_kApplyDip }, { m_saveCopy, s_kSaveCopyDip },
                                                                          { m_undoApplied, s_kUndoDip }, { m_redoApplied, s_kRedoDip },
                                                                          { m_pauseApply, s_kPauseDip }, { m_cancelApply, s_kCancelDip } }};
    int                                                        width = 0;



    for (const auto & [control, widthDip] : isLeft ? std::span<const std::pair<const IDxuiControl *, int>> (left) : std::span<const std::pair<const IDxuiControl *, int>> (right))
    {
        width += control->IsVisible() ? scaler.ToPx (widthDip + s_kGapDip) : 0;
    }

    return width;
}
