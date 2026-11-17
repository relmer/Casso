#pragma once

#include "Pch.h"

class FilesView;
class IInspectorComponent;
class InspectorSession;
class PlatterCorners;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTooltips
//
//  The window's tooltip for the point under the pointer: the first view
//  with one, the File map grid, the window's buttons, then each
//  component. The tooltip is the window's.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorTooltips
{
public:
    struct Sources
    {
        PlatterCorners                         * corners    = nullptr;
        FilesView                              * files      = nullptr;
        const DxuiButton                       * copySector = nullptr;
        const vector<IInspectorComponent *>    * components = nullptr;
    };

    InspectorTooltips (InspectorSession & session, DxuiTooltip & tooltip, const Sources & sources);

    void  Update    (POINT pointPx);
    void  ClearText ()                  { m_text.clear(); }

private:
    std::wstring  GetButtonTip (POINT pointPx, RECT & outAnchorPx) const;

    static bool   IsInside     (const RECT & rect, POINT pointPx) { return PtInRect (&rect, pointPx) != FALSE; }

    InspectorSession  & m_session;
    DxuiTooltip       & m_tooltip;
    Sources             m_sources;
    std::wstring        m_text;
};
