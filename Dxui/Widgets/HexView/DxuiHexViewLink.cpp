#include "Pch.h"

#include "Widgets/HexView/DxuiHexViewLink.h"
#include "Widgets/DxuiHexView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexViewLink::Link
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexViewLink::Link (DxuiHexView & first, DxuiHexView & second)
{
    m_first  = &first;
    m_second = &second;

    first.SetOnSettingsChanged  ([this] () { OnSettings (true); });
    second.SetOnSettingsChanged ([this] () { OnSettings (false); });
    first.SetOnScrolled         ([this] () { OnScrolled (true); });
    second.SetOnScrolled        ([this] () { OnScrolled (false); });

    CopySettings (first, second);
    CopyScroll   (first, second);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexViewLink::CopySettings
//
//  The grouping goes before the bytes per row when the row is set in
//  values, and after it when the row is set in bytes, so each is accepted.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexViewLink::CopySettings (const DxuiHexView & from, DxuiHexView & to)
{
    to.SetShowValues   (from.IsShowingValues());
    to.SetValueFormat  (from.GetValueFormat());
    to.SetTextEncoding (from.GetTextEncoding());
    to.SetZoom         (from.GetZoom());

    if (from.GetColumns() == DxuiHexViewTypes::s_kFixedRowWidth)
    {
        to.SetBytesPerRow (from.GetBytesPerRow());
        (void) to.SetGrouping (from.GetGrouping());
    }
    else
    {
        (void) to.SetGrouping (from.GetGrouping());
        to.SetColumns (from.GetColumns());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexViewLink::CopyScroll
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexViewLink::CopyScroll (const DxuiHexView & from, DxuiHexView & to)
{
    to.SetTopRow (std::min (from.GetTopRow(), to.GetMaxTopRow()));
    to.SetLeftPx (from.GetLeftPx());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexViewLink::OnSettings
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexViewLink::OnSettings (bool fromFirst)
{
    if (m_isCopying || m_first == nullptr)
    {
        return;
    }

    m_isCopying = true;
    CopySettings (fromFirst ? *m_first : *m_second, fromFirst ? *m_second : *m_first);
    CopyScroll   (fromFirst ? *m_first : *m_second, fromFirst ? *m_second : *m_first);
    m_isCopying = false;

    if (m_onSettings)
    {
        m_onSettings();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexViewLink::OnScrolled
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexViewLink::OnScrolled (bool fromFirst)
{
    if (m_isCopying || m_first == nullptr)
    {
        return;
    }

    m_isCopying = true;
    CopyScroll (fromFirst ? *m_first : *m_second, fromFirst ? *m_second : *m_first);
    m_isCopying = false;
}