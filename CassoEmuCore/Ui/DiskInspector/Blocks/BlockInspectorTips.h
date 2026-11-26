#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockInspectorInterfaces.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorTips
//
//  The tip over the block map and the hex view. It appears at once and
//  follows the pointer, as the File map grid's does in the disk inspector,
//  and hides on a press, the wheel, or the pointer leaving.
//
////////////////////////////////////////////////////////////////////////////////

class BlockInspectorTips
{
public:
    BlockInspectorTips (IBlockDisplay & display, IBlockJobs & jobs);

    void  SetPopupHost (DxuiHwndSource * host) { m_tooltip.SetPopupHost (host); }
    void  SetTheme     (const IDxuiTheme & theme) { m_tooltip.SetTheme (theme); }
    void  Layout       (UINT dpi, int widthPx, int heightPx);
    void  Tick         (int64_t nowMs) { m_tooltip.Tick (nowMs); }
    void  OnMouse      (const DxuiMouseEvent & ev);

private:
    void  Update (POINT pointPx);

    IBlockDisplay  & m_display;
    IBlockJobs     & m_jobs;
    DxuiTooltip      m_tooltip;
};
