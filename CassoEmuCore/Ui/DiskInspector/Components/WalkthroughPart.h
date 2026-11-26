#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorWalkthrough.h"
class InspectorSession;





////////////////////////////////////////////////////////////////////////////////
//
//  WalkthroughPart
//
//  The walkthrough over the window (FR-153): while it is open, all input
//  goes to it, and each step's area of the window is outlined.
//
////////////////////////////////////////////////////////////////////////////////

class WalkthroughPart
{
public:
    explicit WalkthroughPart (const InspectorSession & session);

    void  Open   () { m_walkthrough.Open(); }
    bool  IsOpen () const { return m_walkthrough.IsOpen(); }
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & boundsPx);

    bool  OnWalkthroughMouse (const DxuiMouseEvent & ev);
    bool  OnWalkthroughKey   (const DxuiKeyEvent & ev);

private:
    RECT  GetWalkthroughArea (WalkthroughArea area) const;

    const InspectorSession  & m_session;
    InspectorWalkthrough      m_walkthrough;
    DxuiButton                m_close { L"Close" };
    DxuiButton                m_back  { L"Back" };
    DxuiButton                m_next  { L"Next" };
};