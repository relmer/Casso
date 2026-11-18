#pragma once

#include "Pch.h"

struct InspectorCommands;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMenuBar
//
//  The disk inspector's menu bar, built from its InspectorCommands in
//  FR-158's order, with the keys a Windows menu bar has. The track
//  inspector's InspectorMenus and block mode both use it.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorMenuBar
{
public:
    void  Create      (DxuiPanel & parent, DxuiHwndSource * popupHost, IDxuiTextRenderer * text, const InspectorCommands & commands);
    void  Rebuild     (const InspectorCommands & commands);
    void  Layout      (const RECT & area, const RECT & client, const DxuiDpiScaler & scaler);
    int   GetHeightPx (const DxuiDpiScaler & scaler) const;
    void  Tick        (int64_t nowMs);
    bool  OnKey       (const DxuiKeyEvent & ev);
    bool  IsActive    () const;

    const DxuiMenuBar &  GetMenuBar () const { return *m_menuBar; }

    //  The menus from the commands. A command left empty, as Casso's head
    //  commands are in a host without a drive, has no row, and a menu with
    //  no rows isn't added.
    static vector<DxuiMenuBarItem>  BuildItems (const InspectorCommands & commands);

private:
    DxuiMenuBar  * m_menuBar    = nullptr;
    bool           m_isAltAlone = false;
};
