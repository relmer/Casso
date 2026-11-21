#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockInspectorTips.h"
#include "Ui/DiskInspector/Blocks/BlockMapView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorTips::BlockInspectorTips
//
////////////////////////////////////////////////////////////////////////////////

BlockInspectorTips::BlockInspectorTips (IBlockDisplay & display, IBlockJobs & jobs) :
    m_display (display),
    m_jobs    (jobs)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorTips::Layout
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorTips::Layout (UINT dpi, int widthPx, int heightPx)
{
    m_tooltip.SetDpi          (dpi);
    m_tooltip.SetViewportSize (widthPx, heightPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorTips::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorTips::OnMouse (const DxuiMouseEvent & ev)
{
    if (ev.kind == DxuiMouseEventKind::Move)
    {
        Update (ev.positionDip);
    }
    else if (ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Wheel || ev.kind == DxuiMouseEventKind::Leave)
    {
        m_tooltip.RequestHide ((int64_t) GetTickCount64());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorTips::Update
//
//  The map's tip with its swatches, or the hex view's, under the point.
//  None while a job runs.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorTips::Update (POINT pointPx)
{
    std::wstring               tip;
    RECT                       anchor = {};
    vector<DxuiTooltipSwatch>  swatches;
    int64_t                    now    = (int64_t) GetTickCount64();



    if (!m_jobs.IsBusy() && m_display.GetMap().GetTooltip (pointPx, tip, anchor))
    {
        (void) m_display.GetMap().GetTooltipSwatches (pointPx, swatches);
    }
    else if (!m_jobs.IsBusy() && !m_display.GetHexTooltip (pointPx, tip, anchor, swatches))
    {
        tip.clear();
    }

    if (tip.empty())
    {
        m_tooltip.RequestHide (now);
    }
    else
    {
        m_tooltip.SetSwatches    (swatches);
        m_tooltip.RequestShowNow (anchor, tip, now);
    }
}
