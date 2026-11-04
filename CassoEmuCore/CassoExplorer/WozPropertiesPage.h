#pragma once

#include "Pch.h"

#include "CassoExplorer/AppleFilePropertiesDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesPage
//
//  A tab of Disk properties: rows of a label beside fields, as the Apple
//  file properties dialog lays them out.
//
////////////////////////////////////////////////////////////////////////////////

class WozPropertiesPage : public DxuiPropertyPage
{
public:
    explicit WozPropertiesPage (std::wstring title) : DxuiPropertyPage (std::move (title)) {}

    void  Init       (std::vector<AppleFilePropertiesPanel::Row> rows, std::function<void (IDxuiControl *)> onPressed);
    void  SetOnApply (std::function<bool()> fn) { m_onApply = std::move (fn); }
    void  SetRows    (std::vector<AppleFilePropertiesPanel::Row> rows);
    void  Relayout   ();

    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;
    bool  OnApply () override;

private:
    AppleFilePropertiesPanel  m_rows;
    std::function<bool()>     m_onApply;
    RECT                      m_laidOut = {};
    DxuiDpiScaler             m_scaler;
};
