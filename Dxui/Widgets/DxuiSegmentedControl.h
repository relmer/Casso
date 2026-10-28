#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControl
//
//  A capsule divided into equal segments, one of them selected, as iOS's
//  segmented control. The selected segment sits under a raised pill, and a
//  new selection slides the pill from where it is to the new segment; with
//  the system's animations off it jumps. A click selects the segment under
//  it, and Left, Right, Home and End move the selection while the control
//  has focus. The host calls Tick each frame while WantsTick is true.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiSegmentedControl : public IDxuiControl
{
public:
    using ChangeFn = std::function<void (int newIndex)>;

    static constexpr float    kLabelFontDip = 13.0f;
    static constexpr int64_t  kSlideMs      = 220;

    DxuiSegmentedControl();
    ~DxuiSegmentedControl() override = default;

    // Replaces the segments and selects the first, with no slide.
    void  SetSegments (std::vector<std::wstring> labels);

    // A selection made by the host jumps the pill unless isAnimated.
    void  SetSelected (int index, bool isAnimated = false);
    void  SetOnChange (ChangeFn fn) { m_change = std::move (fn); }
    void  SetFocused  (bool focused) { m_focused = focused; }

    // Whether a selection slides the pill; seeded from the system's
    // animation setting.
    void  SetAnimationsEnabled (bool on) { m_animationsEnabled = on; }

    const std::vector<std::wstring> & GetSegments   () const { return m_labels;   }
    int                               GetSelected   () const { return m_selected; }
    int                               GetHoverIndex () const { return m_hover;    }
    bool                              IsFocused     () const { return m_focused;  }

    // Where the pill is, in segments from the first: a whole number at rest,
    // between two while it slides.
    float  GetPillPosition () const { return m_pillPos; }
    bool   WantsTick       () const { return m_isSliding; }

    int    HitTest       (int x, int y) const;
    void   SetMouseHover (int x, int y);
    bool   OnLButtonDown (int x, int y);
    bool   OnLButtonUp   (int x, int y);
    bool   OnKey         (WPARAM vk);

    //
    //  IDxuiControl overrides
    //
    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent   & ev) override;
    void                OnFocusChanged    (bool focused) override { SetFocused (focused); }
    void                Tick              (int64_t nowMs) override;
    std::wstring        GetAccessibleName () const override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::TabStrip; }

private:
    void   Commit      (int newIndex);
    void   StartSlide  ();
    float  GetSegmentWidth () const;

    std::vector<std::wstring>  m_labels;
    ChangeFn                   m_change;
    int                        m_selected          = -1;
    int                        m_hover             = -1;
    int                        m_pressed           = -1;
    bool                       m_focused           = false;
    bool                       m_animationsEnabled = true;
    bool                       m_isSliding         = false;
    float                      m_pillPos           = 0.0f;
    float                      m_slideFrom         = 0.0f;
    int64_t                    m_slideStartMs      = -1;
    DxuiDpiScaler              m_scaler;
};
