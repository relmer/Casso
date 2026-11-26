#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockHeaderText.h"

class BlockJobQueue;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditState
//
//  What the block pane's edit controls have for the current block: whether
//  it can be edited and is, the bytes changed in it, whether B's bytes can
//  be put into it while comparing, and the last save's result.
//
////////////////////////////////////////////////////////////////////////////////

struct BlockEditState
{
    bool          canEdit     = false;
    bool          isEditing   = false;
    int           blockBytes  = 0;
    bool          canUndo     = false;
    bool          canUseOther = false;
    std::wstring  note;

    bool operator== (const BlockEditState & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderView
//
//  The header over block mode's hex view: the previous and next block
//  buttons, "Block 2" and its capsules, Edit and Copy block in the title
//  row, and under it a row of the same height for the block's edit
//  controls, the field line and the last save's result, or a long job's
//  progress and Cancel. The second row is there whether or not the block is
//  being edited, so the hex view doesn't move. The controls are children of
//  the panel that creates the header.
//
////////////////////////////////////////////////////////////////////////////////

class BlockHeaderView
{
public:
    static constexpr float  kPadDip    = 8.0f;
    static constexpr float  kRowDip    = 28.0f;
    static constexpr float  kTitleDip  = 16.0f;
    static constexpr float  kFontDip   = 12.0f;
    static constexpr float  kBarDip    = 240.0f;
    static constexpr float  kButtonDip = 110.0f;
    static constexpr float  kEditDip   = 72.0f;
    static constexpr float  kUndoDip   = 150.0f;

    using StepFn   = std::function<void (int64_t delta)>;
    using ActionFn = std::function<void ()>;

    //  The controls created on `parent`. Previous and Next call `step`, Copy
    //  block calls `copy`, and Cancel calls `cancel`.
    void  Create (DxuiPanel & parent, StepFn step, ActionFn copy, ActionFn cancel);

    //  Edit or Done, Undo block changes, and Use B's bytes.
    void  SetOnEdit     (ActionFn fn) { m_onEdit     = std::move (fn); }
    void  SetOnUndo     (ActionFn fn) { m_onUndo     = std::move (fn); }
    void  SetOnUseOther (ActionFn fn) { m_onUseOther = std::move (fn); }

    //  Lays the header out across `column` from `top`, with the progress in
    //  the second row during a long job. Returns the y under the header,
    //  which is the same whatever the edit controls are.
    int  Layout (const RECT & column, int top, bool isLongJob, const DxuiDpiScaler & scaler);

    //  The block's number and capsules, the field line, and whether there is
    //  a block before and after it.
    void  ShowBlock    (uint32_t block, vector<BlockCapsule> capsules);
    void  SetFieldLine (std::wstring line)            { m_fieldLine = std::move (line); }
    void  SetSteps     (bool hasPrevious, bool hasNext);

    //  Copy block and the edit controls are disabled during a long job.
    //  Cancel is visible for a long job that can be canceled.
    void  SetBusy (bool isLongJob, bool canCancel);

    //  The edit controls. True when the controls in the second row change,
    //  so the panel lays the header out again.
    bool  SetEditState (const BlockEditState & state);

    //  "3 bytes changed in this block", or empty with none.
    static std::wstring  FormatBlockCount (int bytes);

    DxuiButton  & GetCancel () const { return *m_cancel; }
    DxuiButton  & GetEdit   () const { return *m_edit; }
    DxuiButton  & GetUndo   () const { return *m_undo; }

    //  The title row and the second row for a block of an open image, and
    //  the progress during a long job.
    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiDpiScaler & scaler, const BlockJobQueue & jobs, bool isOpen) const;

private:
    void  PaintTitle    (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiDpiScaler & scaler) const;
    void  PaintInfo     (IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiDpiScaler & scaler) const;
    void  PaintProgress (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiDpiScaler & scaler, const BlockJobQueue & jobs) const;

    static constexpr const wchar_t *  s_kpszFont       = L"Segoe UI";
    static constexpr const wchar_t *  s_kpszGlyphPrev  = L"\xE76B";
    static constexpr const wchar_t *  s_kpszGlyphNext  = L"\xE76C";

    DxuiIconButton        * m_previous   = nullptr;
    DxuiIconButton        * m_next       = nullptr;
    DxuiButton            * m_edit       = nullptr;
    DxuiButton            * m_copy       = nullptr;
    DxuiButton            * m_undo       = nullptr;
    DxuiButton            * m_useOther   = nullptr;
    DxuiButton            * m_cancel     = nullptr;
    ActionFn                m_onEdit;
    ActionFn                m_onUndo;
    ActionFn                m_onUseOther;
    BlockEditState          m_state;
    bool                    m_isLongJob  = false;
    std::wstring            m_title;
    vector<BlockCapsule>    m_capsules;
    std::wstring            m_fieldLine;
    RECT                    m_titleRect  = {};
    RECT                    m_rowRect    = {};
    RECT                    m_infoRect   = {};
};
