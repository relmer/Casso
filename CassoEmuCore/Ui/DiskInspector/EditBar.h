#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EditBarState
//
//  What the edit bar has for the selected sector and the disk's pending
//  edits (FR-096 to FR-098, FR-100 to FR-102, FR-107).
//
////////////////////////////////////////////////////////////////////////////////

struct EditBarState
{
    bool          canEdit          = false;
    bool          isEditing        = false;
    bool          hasSectorEdit    = false;
    bool          isChecksumOn     = true;
    bool          isChecksumLocked = false;
    int           bytes            = 0;
    int           sectors          = 0;
    bool          canApply         = false;
    bool          canUndoApplied   = false;
    bool          canRedoApplied   = false;
    std::wstring  note;
    std::wstring  applyBlocked;
};





////////////////////////////////////////////////////////////////////////////////
//
//  EditBar
//
//  The row of editing controls above the Sector data tab's bytes: "Edit
//  sector", "Recompute the checksum" for the selected sector, the count of
//  bytes and sectors changed, "Discard sector", "Discard", "Apply...",
//  "Save edited copy...", "Undo applied edit" and "Redo applied edit". The
//  note between them has why the sector cannot be edited, why "Apply..." is
//  unavailable, or the result of the last apply.
//
////////////////////////////////////////////////////////////////////////////////

class EditBar : public DxuiPanel
{
public:
    using ActionFn = std::function<void ()>;
    using CheckFn  = std::function<void (bool isChecked)>;

    static constexpr int  kRowDip = 34;

    EditBar();

    void  SetOnEdit          (ActionFn fn) { m_onEdit          = std::move (fn); }
    void  SetOnChecksum      (CheckFn fn)  { m_onChecksum      = std::move (fn); }
    void  SetOnDiscardSector (ActionFn fn) { m_onDiscardSector = std::move (fn); }
    void  SetOnDiscard       (ActionFn fn) { m_onDiscard       = std::move (fn); }
    void  SetOnApply         (ActionFn fn) { m_onApply         = std::move (fn); }
    void  SetOnSaveCopy      (ActionFn fn) { m_onSaveCopy      = std::move (fn); }
    void  SetOnUndoApplied   (ActionFn fn) { m_onUndoApplied   = std::move (fn); }
    void  SetOnRedoApplied   (ActionFn fn) { m_onRedoApplied   = std::move (fn); }

    void  SetState (const EditBarState & state);

    //  The tooltip for the control under a point, with why it is unavailable
    //  when it is; empty when no control is there.
    std::wstring  GetTip (POINT pointPx, RECT & outAnchorPx) const;

    //  "3 bytes in 1 sector changed", or empty with no changes.
    static std::wstring  FormatCount (int bytes, int sectors);

    //  One row when every visible control fits the width, else two, with the
    //  disk's controls on the second.
    int   GetRowCount (int widthPx, const DxuiDpiScaler & scaler) const;
    int   GetHeightPx (int widthPx, const DxuiDpiScaler & scaler) const { return GetRowCount (widthPx, scaler) * scaler.ToPx (kRowDip); }

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;

private:
    int   GetGroupWidthPx (bool isLeft, const DxuiDpiScaler & scaler) const;

private:
    DxuiButton    * m_edit          = nullptr;
    DxuiCheckbox  * m_checksum      = nullptr;
    DxuiLabel     * m_count         = nullptr;
    DxuiLabel     * m_note          = nullptr;
    DxuiButton    * m_discardSector = nullptr;
    DxuiButton    * m_discard       = nullptr;
    DxuiButton    * m_apply         = nullptr;
    DxuiButton    * m_saveCopy      = nullptr;
    DxuiButton    * m_undoApplied   = nullptr;
    DxuiButton    * m_redoApplied   = nullptr;
    ActionFn        m_onEdit;
    CheckFn         m_onChecksum;
    ActionFn        m_onDiscardSector;
    ActionFn        m_onDiscard;
    ActionFn        m_onApply;
    ActionFn        m_onSaveCopy;
    ActionFn        m_onUndoApplied;
    ActionFn        m_onRedoApplied;
    DxuiDpiScaler   m_layoutScaler;
    bool            m_hasLayout     = false;
    std::wstring    m_applyBlocked;
};
