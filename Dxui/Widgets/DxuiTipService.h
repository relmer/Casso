#pragma once

#include "Pch.h"
#include "Core/DxuiTip.h"


class DxuiTooltip;
class IDxuiControl;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTipService
//
//  The tip under the pointer, displayed in the window's one tooltip, as with
//  the system's tooltip control and the tools of its window. Controls return
//  their tips from IDxuiControl::TryGetTip. On each pointer move the window
//  calls Update, or finds the tip itself and calls Show or Hide. Each tip's
//  placement, lifetime and look are applied, and:
//
//  - a repeat of the tip that is up, or of the tip whose delay is running,
//    changes nothing;
//  - after a press the tip is hidden at once, and that tip does not open
//    again until the pointer is over another tip or none;
//  - while a menu is open there is no tip;
//  - after a pending tip, the tip that is up stays up.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTipService
{
public:
    explicit DxuiTipService (DxuiTooltip & tooltip) : m_tooltip (tooltip) {}

    //  The tip of the front control under a point in root's tree.
    static bool  FindTip      (const IDxuiControl & root, POINT pointPx, DxuiTip & tip);

    void  Update             (const IDxuiControl & root, POINT pointPx, int64_t nowMs);
    void  Show               (const DxuiTip & tip, POINT pointPx, int64_t nowMs);
    void  Hide               (int64_t nowMs);
    void  OnPress            ();
    void  OnLeave            (int64_t nowMs);
    void  SetSuppressed      (bool isSuppressed);
    bool  IsSuppressed       () const { return m_isSuppressed; }

    //  Ticks the tooltip while it has a timer or animation running; true when
    //  it did.
    bool  Tick               (int64_t nowMs);
    bool  WantsTick          () const;

    DxuiTooltip &  GetTooltip () { return m_tooltip; }

private:
    void  ApplyLook          (const DxuiTip & tip);
    bool  IsPressedTip       (const DxuiTip & tip) const;

    DxuiTooltip   & m_tooltip;
    bool            m_isSuppressed  = false;
    bool            m_isPressHidden = false;
    std::wstring    m_pressedText;
    std::wstring    m_lastText;
};
