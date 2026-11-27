#pragma once

#include "Pch.h"

class FilesView;
class IInspectorComponent;
class InspectorSession;
class InspectorTip;
class PlatterCorners;
struct SideNames;





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
    std::wstring  GetButtonTip     (POINT pointPx, RECT & outAnchorPx) const;
    void          BuildComparedTip (POINT pointPx, bool isPlatter, std::wstring & inOutTip, vector<DxuiTooltipSwatch> & inOutSwatches, vector<vector<std::wstring>> & outTable) const;

    static void   MergeTips        (const InspectorTip (& tips)[2], const SideNames & names, std::wstring & outTip, vector<DxuiTooltipSwatch> & outSwatches,
                                    vector<vector<std::wstring>> & outTable);

    static bool   IsInside     (const RECT & rect, POINT pointPx) { return PtInRect (&rect, pointPx) != FALSE; }

    InspectorSession  & m_session;
    DxuiTooltip       & m_tooltip;
    Sources             m_sources;
    std::wstring        m_text;
};
