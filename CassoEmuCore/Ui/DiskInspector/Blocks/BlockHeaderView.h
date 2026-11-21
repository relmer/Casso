#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockHeaderText.h"

class BlockJobQueue;
class EditBar;
struct EditBarState;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderView
//
//  The header over block mode's hex view: the previous and next block
//  buttons, "Block 2" and its capsules, Copy block, the field line, and a
//  row with the pending edits or a long job's progress and Cancel. The
//  controls are children of the panel that creates the header.
//
////////////////////////////////////////////////////////////////////////////////

class BlockHeaderView
{
public:
    static constexpr float  kPadDip    = 8.0f;
    static constexpr float  kRowDip    = 28.0f;
    static constexpr float  kLineDip   = 20.0f;
    static constexpr float  kTitleDip  = 16.0f;
    static constexpr float  kFontDip   = 12.0f;
    static constexpr float  kBarDip    = 240.0f;
    static constexpr float  kButtonDip = 110.0f;

    using StepFn   = std::function<void (int64_t delta)>;
    using ActionFn = std::function<void ()>;

    //  The controls created on `parent`. Previous and Next call `step`, Copy
    //  block calls `copy`, and Cancel calls `cancel`.
    void  Create (DxuiPanel & parent, StepFn step, ActionFn copy, ActionFn cancel);

    //  Lays the header out across `column` from `top`, with the progress row
    //  in place of the edit bar during a long job. Returns the y under the
    //  header.
    int  Layout (const RECT & column, int top, bool isLongJob, const DxuiDpiScaler & scaler);

    //  The block's number and capsules, the field line, and whether there is
    //  a block before and after it.
    void  ShowBlock    (uint32_t block, vector<BlockCapsule> capsules);
    void  SetFieldLine (std::wstring line)            { m_fieldLine = std::move (line); }
    void  SetSteps     (bool hasPrevious, bool hasNext);

    //  Copy block and the edit bar are disabled during a long job. Cancel is
    //  visible for a long job that can be canceled.
    void  SetBusy (bool isLongJob, bool canCancel);

    //  The edit bar's controls. True when the bar's height changes, so the
    //  panel is laid out again.
    bool  SetEditState (const EditBarState & state, const DxuiDpiScaler & scaler);

    EditBar     & GetEditBar () const { return *m_editBar; }
    DxuiButton  & GetCancel  () const { return *m_cancel; }

    //  The title row and the field line for a block of an open image, and
    //  the progress row during a long job.
    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiDpiScaler & scaler, const BlockJobQueue & jobs, bool isOpen) const;

private:
    void  PaintTitle    (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiDpiScaler & scaler) const;
    void  PaintProgress (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiDpiScaler & scaler, const BlockJobQueue & jobs) const;

    static constexpr const wchar_t *  s_kpszFont       = L"Segoe UI";
    static constexpr const wchar_t *  s_kpszGlyphPrev  = L"\xE76B";
    static constexpr const wchar_t *  s_kpszGlyphNext  = L"\xE76C";

    DxuiIconButton        * m_previous  = nullptr;
    DxuiIconButton        * m_next      = nullptr;
    DxuiButton            * m_copy      = nullptr;
    EditBar               * m_editBar   = nullptr;
    DxuiButton            * m_cancel    = nullptr;
    std::wstring            m_title;
    vector<BlockCapsule>    m_capsules;
    std::wstring            m_fieldLine;
    RECT                    m_titleRect = {};
    RECT                    m_fieldRect = {};
    RECT                    m_rowRect   = {};
};
