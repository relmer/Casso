#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CardLayoutModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutBar
//
//  The card drawn to scale as a bar of its CFFA volume slots, the space the
//  CFFA layout doesn't map, and a FAT32 slot at the right end; a pointed
//  puck under the bar at the chosen position; the position's description
//  and space; and each adapter's support in a grid under them.
//
//  The puck moves by dragging or clicking along the bar, and by Left,
//  Right, Home and End. A press or drag past the last volume slot snaps to
//  FAT32, and the FAT32 slot then grows to fill the bar. From FAT32, a drag
//  returns to volumes only once it is back past the middle of the volume
//  slots, so a wobble at the edge doesn't flick between the two.
//
////////////////////////////////////////////////////////////////////////////////

class CardLayoutBar : public IDxuiControl
{
public:
    using ChangeFn = std::function<void (int position)>;

    explicit CardLayoutBar (const CardLayoutModel & model);

    void  SetPosition (int position);
    int   GetPosition () const { return m_position; }
    void  SetOnChange (ChangeFn fn) { m_change = std::move (fn); }

    //  True while the FAT32 slot is growing or shrinking. `nowMs` advances it.
    bool  AdvanceAnimation (int64_t nowMs);

    //  The control's height at a width, in pixels.
    int   MeasureHeightPx (int widthPx, const DxuiDpiScaler & scaler) const;

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent & ev) override;
    void                OnFocusChanged    (bool focused) override { m_focused = focused; }
    std::wstring        GetAccessibleName () const override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Slider; }

private:
    static constexpr float     kBarHeightDip    = 34.0f;
    static constexpr float     kPuckWidthDip    = 14.0f;
    static constexpr float     kPuckHeightDip   = 20.0f;
    static constexpr float     kPuckTipDip      = 7.0f;
    static constexpr float     kLineDip         = 20.0f;
    static constexpr float     kRowDip          = 36.0f;
    static constexpr float     kGapDip          = 6.0f;
    static constexpr float     kFontDip         = 13.0f;
    static constexpr float     kSmallFontDip    = 11.0f;
    static constexpr float     kFat32Share      = 0.08f;
    static constexpr float     kAdapterShare    = 0.42f;
    static constexpr float     kIconDip         = 24.0f;
    static constexpr int64_t   kAnimationMs     = 320;

    static constexpr uint32_t  s_kUniversalArgb   = 0xFF639922;
    static constexpr uint32_t  s_kSetupArgb       = 0xFFEF9F27;
    static constexpr uint32_t  s_kSetupStripeArgb = 0xFFFAC775;
    static constexpr uint32_t  s_kUnusedArgb      = 0xFF5F5E5A;
    static constexpr uint32_t  s_kUnavailableArgb = 0xFF444441;
    static constexpr uint32_t  s_kFat32Argb       = 0xFF97C459;
    static constexpr uint32_t  s_kFat32OnArgb     = 0xFF3B6D11;
    static constexpr uint32_t  s_kWorksArgb       = 0xFF3B6D11;
    static constexpr uint32_t  s_kStepArgb        = 0xFFFFB900;   // Windows' warning yellow
    static constexpr uint32_t  s_kWarningMarkArgb = 0xFF000000;
    //  Dark enough for the white cross, in light and dark themes.
    static constexpr uint32_t  s_kFailsArgb       = 0xFFA32D2D;
    static constexpr uint32_t  s_kLightTextArgb   = 0xFFEAF3DE;
    static constexpr uint32_t  s_kDarkTextArgb    = 0xFF173404;

    int    GetPositionAt   (int xPx) const;
    float  GetSlotWidthPx  () const;
    float  GetPuckCenterPx () const;
    void   Choose          (int position);

    void   PaintBar   (IDxuiPainter & painter, IDxuiTextRenderer & text);
    void   PaintPuck  (IDxuiPainter & painter, const IDxuiTheme & theme);
    void   PaintGrid  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, float top);
    void   PaintHatch (IDxuiPainter & painter, float x, float y, float width, float height);

    const CardLayoutModel  & m_model;
    DxuiDpiScaler            m_scaler;
    ChangeFn                 m_change;
    int                      m_position      = 1;
    bool                     m_dragging      = false;
    bool                     m_focused       = false;
    float                    m_fat32Progress = 0.0f;   // 0: a slot at the right end; 1: the whole bar
    float                    m_fromProgress  = 0.0f;
    int64_t                  m_animationMs   = 0;      // when the slot started growing or shrinking; 0 for none
};
